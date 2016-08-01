#include "graph_config.h"

#include <map>
#include <iostream>
#include <fstream>
#include <chrono>

#include <boost/graph/distributed/page_rank.hpp>
#include <boost/graph/distributed/betweenness_centrality.hpp>
#include <boost/graph/distributed/connected_components_parallel_search.hpp>

#include <boost/graph/parallel/basic_reduce.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/property_map/parallel/distributed_property_map.hpp>

#include <dynograph_util.hh>
#include <hooks.h>

using std::cerr;
using std::ifstream;
using std::map;
using std::chrono::steady_clock;
using std::chrono::duration;
using std::string;

void printTime(string stepDesc, duration<double, std::milli> diff)
{
    cerr << stepDesc << ": " << diff.count() << " ms.\n";
}

struct Args
{
    string algName;
    string inputPath;
    int64_t windowSize;
    int64_t numBatches;
    int64_t numTrials;
    int64_t enableDeletions;
};

Args
getArgs(int argc, char **argv)
{
    Args args;
    if (argc != 6)
    {
        cerr << "Usage: alg_name input_path num_batches window_size num_trials \n";
        exit(-1);
    }

    args.algName = argv[1];
    args.inputPath = argv[2];
    args.numBatches = atoll(argv[3]);
    args.windowSize = atoll(argv[4]);
    args.numTrials = atoll(argv[5]);
    if (args.windowSize < 0)
    {
        args.enableDeletions = 1;
        args.windowSize = -args.windowSize;
    } else { args.enableDeletions = 0; }
    if (args.numBatches < 1 || args.windowSize < 1 || args.numTrials < 1)
    {
        cerr << "num_batches, window_size, and num_trials must be positive\n";
        exit(-1);
    }

    return args;
}

void insertBatch(DynoGraph::Batch batch, Graph &g)
{
    // TODO Parallel graph load
    for (DynoGraph::Edge e : batch)
    {

        VertexId Src = boost::vertex(e.src, g);
        VertexId Dst = boost::vertex(e.dst, g);
        // Only insert edge if this process owns the source vertex
        //if (Src.owner == process_id(g.process_group())) {
            // Try to insert the edge
            std::pair<Edge, bool> inserted_edge = boost::add_edge(
                    Src, Dst,
                    // Boost uses template nesting to implement multiple edge properties
                    Weight(e.weight, Timestamp(e.timestamp)),
                    g);
            // If the edge already existed...
            if (!inserted_edge.second) {
                Edge &existing_edge = inserted_edge.first;
                // Increment edge weight
                int64_t weight = get(boost::edge_weight, g, existing_edge);
                weight += e.weight;
                put(boost::edge_weight, g, existing_edge, weight);
                // Overwrite timestamp
                put(boost::edge_timestamp, g, existing_edge, e.timestamp);
            }
        //}
    }
}

void deleteEdges(int64_t threshold, Graph &g)
{
    // TODO Make sure this is actually running in parallel
    boost::remove_edge_if(
        [&](const Edge &e)
        {
            return get(boost::edge_timestamp, g, e) < threshold;
        }
    ,g);
}

//std::vector<VertexId> bfsRoots = {3, 30, 300, 4, 40, 400};
std::vector<string> algs = {"all", "bc", "cc", "pagerank"};

void runAlgorithm(string algName, Graph &g, int64_t trial)
{
    cerr << "Running " << algName << "...\n";

    if (algName == "all")
    {
        for (string alg : algs)
        {
            runAlgorithm(alg, g, trial);
        }
    }

    else if (algName == "bc")
    {
        std::vector<int64_t> local_centrality_vec(boost::num_vertices(g), 0);
        boost::brandes_betweenness_centrality(
            g,
            make_iterator_property_map(local_centrality_vec.begin(), get(boost::vertex_index, g))
        );
    }

    else if (algName == "cc")
    {
        // FIXME use incremental_components here instead (st_components)
        std::vector<int> local_components_vec(boost::num_vertices(g));
        //typedef boost::iterator_property_map<std::vector<int>::iterator, boost::property_map<Graph, boost::vertex_index_t>::type> ComponentMap;
        //ComponentMap component(local_components_vec.begin(), get(boost::vertex_index, g));
        //boost::graph::distributed::connected_components_ps(g, component);
        boost::graph::distributed::connected_components_ps(
            g,
            boost::parallel::make_iterator_property_map(local_components_vec.begin(), get(boost::vertex_index, g))
        );

    }

    else if (algName == "pagerank")
    {
        std::vector<double> vertexRanks(num_vertices(g));
        std::vector<double> vertexRanks2(num_vertices(g));

        boost::graph::distributed::page_rank(
            g,
            make_iterator_property_map(vertexRanks.begin(), get(boost::vertex_index, g)),
            boost::graph::n_iterations(20),
            0.85,
            boost::num_vertices(g),
            make_iterator_property_map(vertexRanks2.begin(), get(boost::vertex_index, g))
        );

    }

    else
    {
        cerr << "Algorithm " << algName << " not implemented!\n";
        exit(-1);
    }
}

#define MASTER_PROCESS (boost::graph::distributed::process_id(g.process_group()) == 0)

int main(int argc, char *argv[]) {
    // Initialize MPI
    boost::mpi::environment env(argc, argv);
    ProcessGroup pg;

    // Parse the command line
    Args args = getArgs(argc, argv);

    // Create the graph
    // TODO scale this up with available system memory
    Graph::vertices_size_type max_num_vertices = 100001;
    Graph g(max_num_vertices, pg);

    DynoGraph::Dataset *dataset;

    // Master process pre-loads the batches
    if (MASTER_PROCESS)
    {
        // Pre-load the edge batches
        cerr << "Pre-loading " << args.inputPath << " from disk...\n";
        auto t1 = steady_clock::now();

        dataset = new DynoGraph::Dataset(args.inputPath, args.numBatches);

        auto t2 = steady_clock::now();
        printTime("Graph pre-load", t2 - t1);

    // Other processes just wait for information
    }
    synchronize(pg);

    for (int64_t trial = 0; trial < args.numTrials; ++trial)
    {
        // Ingest each batch and run analytics
        for (int batchId = 0; batchId < args.numBatches; ++batchId)
        {
            // Deletions
            if (args.enableDeletions)
            {
                if (MASTER_PROCESS)
                {
                    int64_t modified_after = dataset->getTimestampForWindow(batchId, args.windowSize);
                    cerr << "Deleting edges older than " << modified_after << "\n";
                    Hooks::getInstance().region_begin("deletions", trial);
                    deleteEdges(modified_after, g);
                    Hooks::getInstance().region_end("deletions", trial);
                }
                synchronize(pg);
            }

            // Batch insertion
            if (MASTER_PROCESS)
            {
                cerr << "Loading batch " << batchId << "...\n";
                Hooks::getInstance().region_begin("insertions", trial);
                insertBatch(dataset->getBatch(batchId), g);
                Hooks::getInstance().region_end("insertions", trial);
            }
            synchronize(pg);
            cerr << "P" << process_id(pg) << ": Number of vertices: " << num_vertices(g) << "\n";
            cerr << "P" << process_id(pg) << ": Number of edges: " << num_edges(g) << "\n";

            // Algorithm
            Hooks::getInstance().region_begin(args.algName, trial);
            runAlgorithm(args.algName, g, trial);
            Hooks::getInstance().region_end(args.algName, trial);

        }
    }
    if (MASTER_PROCESS) { delete dataset; }

    return 0;
}