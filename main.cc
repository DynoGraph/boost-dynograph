#include "graph_config.h"

#include <chrono>
#include <dynograph_util.hh>
#include "distributed_dataset.h"
#include <hooks.h>
#include "boost_algs.h"

using std::cerr;
using std::ifstream;
using std::map;
using std::chrono::steady_clock;
using std::chrono::duration;
using std::string;
using std::unique_ptr;

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

    // Pre-load the edge batches
    if (process_id(pg) == 0) { cerr << "Pre-loading " << args.inputPath << " from disk...\n"; }
    auto t1 = steady_clock::now();

    unique_ptr<DynoGraph::Dataset> dataset = DynoGraph::loadDatasetDistributed(
            args.inputPath, args.numBatches, communicator(pg));

    auto t2 = steady_clock::now();
    printTime("Graph pre-load", t2 - t1);
    cerr << "P" << process_id(pg) << ": loaded " << dataset->edges.size() << " edges\n";

    for (int64_t trial = 0; trial < args.numTrials; ++trial)
    {
        // Ingest each batch and run analytics
        for (int batchId = 0; batchId < args.numBatches; ++batchId)
        {
            // Deletions
            if (args.enableDeletions)
            {
                if (process_id(pg) == 0)
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
            if (process_id(pg) == 0) { cerr << "Loading batch " << batchId << "...\n"; }
            Hooks::getInstance().region_begin("insertions", trial);
            insertBatch(dataset->getBatch(batchId), g);
            Hooks::getInstance().region_end("insertions", trial);

            synchronize(pg);
            cerr << "P" << process_id(pg) << ": Number of vertices: " << num_vertices(g) << "\n";
            cerr << "P" << process_id(pg) << ": Number of edges: " << num_edges(g) << "\n";

            // Algorithm
            Hooks::getInstance().region_begin(args.algName, trial);
            runAlgorithm(args.algName, g, trial);
            Hooks::getInstance().region_end(args.algName, trial);

        }
    }

    return 0;
}