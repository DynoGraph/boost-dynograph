#include "graph_config.h"

#include <map>
#include <iostream>
#include <fstream>
#include <chrono>

#include <boost/graph/edge_list.hpp>
#include <boost/graph/directed_graph.hpp>
#include <boost/graph/page_rank.hpp>
#include <boost/graph/betweenness_centrality.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/clustering_coefficient.hpp>

#include <boost/property_map/property_map.hpp>

#include <dynograph_util.hh>
#include <hooks.h>

using std::cerr;
using std::cerr;
using std::ifstream;
using std::map;
using std::chrono::steady_clock;
using std::chrono::duration;
using std::chrono::steady_clock;
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
    for (DynoGraph::Edge e : batch)
    {
        // Try to insert the edge
        auto inserted_edge = add_edge(
            e.src,
            e.dst,
            // Boost uses template nesting to implement multiple edge properties
            Weight(e.weight, Timestamp(e.timestamp)),
            g);
        // If the edge already existed...
        if (!inserted_edge.second)
        {
            Edge &existing_edge = inserted_edge.first;
            // Increment edge weight
            int64_t weight = get(boost::edge_weight, g, existing_edge);
            weight += e.weight;
            put(boost::edge_weight, g, existing_edge, weight);
            // Overwrite timestamp
            put(boost::edge_timestamp, g, existing_edge, e.timestamp);
        }
    }
}

void deleteEdges(int64_t threshold, Graph &g)
{
    remove_edge_if(
        [&](const Edge &e)
        {
            return get(boost::edge_timestamp, g, e) < threshold;
        }
    ,g);
}

std::vector<VertexId> bfsRoots = {3, 30, 300, 4, 40, 400};
std::vector<string> algs = {"bfs", "bc", "cc", "clustering", "pagerank"};

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

    else if (algName == "bfs")
    {
        for (VertexId root : bfsRoots)
        {
            boost::graph::breadth_first_search(g, root);
        }
    }

    else if (algName == "bc")
    {
        std::map<VertexId, double> vertexCentrality;
        boost::associative_property_map<std::map<VertexId, double>> centrality_map(vertexCentrality);
        boost::brandes_betweenness_centrality(g, centrality_map);
    }

    else if (algName == "cc")
    {
        // FIXME use incremental_components here instead
        std::map<VertexId, int64_t> vertexComponents;
        boost::associative_property_map<std::map<VertexId, int64_t>> component_map(vertexComponents);
        boost::connected_components(g, component_map);
    }

    else if (algName == "clustering")
    {
        std::map<VertexId, int64_t> vertexCoefficients;
        boost::associative_property_map<std::map<VertexId, int64_t>> coefficients_map(vertexCoefficients);
        boost::all_clustering_coefficients(g, coefficients_map);
    }

    else if (algName == "pagerank")
    {
        std::map<VertexId, double> vertexRanks;
        boost::associative_property_map<std::map<VertexId, double>> rank_map(vertexRanks);
        boost::graph::page_rank(g, rank_map, boost::graph::n_iterations(20), 0.85);
    }

    else
    {
        cerr << "Algorithm " << algName << " not implemented!\n";
        exit(-1);
    }
}


int main(int argc, char *argv[]) {
    // Parse the command line
    Args args = getArgs(argc, argv);

    // Create the graph
    // TODO scale this up with available system memory
    Graph::vertices_size_type max_num_vertices = 10001;
    Graph g(max_num_vertices);

    // Pre-load the edge batches
    cerr << "Pre-loading " << args.inputPath << " from disk...\n";
    auto t1 = steady_clock::now();

    DynoGraph::Dataset dataset(args.inputPath, args.numBatches);

    auto t2 = steady_clock::now();
    printTime("Graph pre-load", t2 - t1);

    for (int64_t trial = 0; trial < args.numTrials; ++trial)
    {
        // Ingest each batch and run analytics
        for (int batchId = 0; batchId < dataset.getNumBatches(); ++batchId)
        {
            // Deletions
            int64_t modified_after = dataset.getTimestampForWindow(batchId, args.windowSize);
            if (args.enableDeletions)
            {
                cerr << "Deleting edges older than " << modified_after << "\n";
                Hooks::getInstance().region_begin("deletions", trial);
                deleteEdges(modified_after, g);
                Hooks::getInstance().region_end("deletions", trial);
            }

            // Batch insertion
            cerr << "Loading batch " << batchId << "...\n";
            Hooks::getInstance().region_begin("insertions", trial);
            insertBatch(dataset.getBatch(batchId), g);
            Hooks::getInstance().region_end("insertions", trial);

            cerr << "Number of vertices: " << g.m_vertices.size() << "\n";
            cerr << "Number of edges: " << g.m_edges.size() << "\n";

            // Algorithm
            Hooks::getInstance().region_begin(args.algName, trial);
            runAlgorithm(args.algName, g, trial);
            Hooks::getInstance().region_end(args.algName, trial);

        }
    }

    return 0;
}