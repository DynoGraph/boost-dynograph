#include "boost_algs.h"

#include <dynograph_util.hh>
#include "distributed_dataset.h"
#include <hooks.h>

#include <chrono>
#include <sstream>
#include <vector>

using std::cout;
using std::cerr;
using std::ifstream;
using std::chrono::steady_clock;
using std::chrono::duration;
using std::string;
using std::unique_ptr;
using std::vector;
using std::stringstream;

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
    int64_t maxNumVertices;
};

Args
getArgs(int argc, char **argv)
{
    Args args;
    if (argc != 7)
    {
        cerr << "Usage: alg_name input_path num_batches window_size num_trials max_nv \n";
        exit(-1);
    }

    args.algName = argv[1];
    args.inputPath = argv[2];
    args.numBatches = atoll(argv[3]);
    args.windowSize = atoll(argv[4]);
    args.numTrials = atoll(argv[5]);
    args.maxNumVertices = atoll(argv[6]);

    if (args.windowSize < 0)
    {
        args.enableDeletions = 1;
        args.windowSize = -args.windowSize;
    } else { args.enableDeletions = 0; }
    if (args.numBatches < 1 || args.windowSize < 1 || args.numTrials < 1 || args.maxNumVertices < 1)
    {
        cerr << "num_batches, window_size, num_trials, and max_nv must be positive\n";
        exit(-1);
    }


    return args;
}

void insertBatch(DynoGraph::Batch batch, Graph &g, Graph::vertices_size_type max_nv)
{
    // TODO Parallel graph load
    for (DynoGraph::Edge e : batch)
    {
        assert(e.src < max_nv && e.dst < max_nv);
        Hooks::getInstance().traverse_edge();
        VertexId Src = boost::vertex(e.src, g);
        VertexId Dst = boost::vertex(e.dst, g);
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

void reportOwnership(Graph &g)
{
    int64_t edgeCount = 0;
    int64_t myEdgeCount = 0;
    auto allEdges = edges(g);
    for (auto e = allEdges.first; e != allEdges.second; ++e)
    {
        if (e->owner() == g.process_group().rank)
        {
            edgeCount += 1;
        }
        myEdgeCount += 1;
    }

    cerr << "P" << g.process_group().rank
         << ": total edges = " << edgeCount
         << " my edges = " << myEdgeCount << std::endl;
    synchronize(g.process_group());
}


// Helper functions to split strings
// http://stackoverflow.com/a/236803/1877086
void split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
}
vector<string> split(const string &s, char delim) {
    vector<string> elems;
    split(s, delim, elems);
    return elems;
}


int main(int argc, char *argv[]) {
    // Initialize MPI
    boost::mpi::environment env(argc, argv);
    ProcessGroup pg;

    // Don't flush output after each operation. Helps keep per-process output together
    cerr << std::nounitbuf;

    // Parse the command line
    Args args = getArgs(argc, argv);

    // Create the graph
    // TODO use a round-robin distribution strategy, then over-provision based on available memory|
    Graph::vertices_size_type max_num_vertices = static_cast<Graph::vertices_size_type>(args.maxNumVertices);
    Graph g(max_num_vertices, pg);

    // Pre-load the edge batches
    if (process_id(pg) == 0) { cerr << "Pre-loading " << args.inputPath << " from disk...\n"; }
    auto t1 = steady_clock::now();

    unique_ptr<DynoGraph::Dataset> dataset = DynoGraph::loadDatasetDistributed(
            args.inputPath, args.numBatches, communicator(pg));

    auto t2 = steady_clock::now();
    if (process_id(pg) == 0) { printTime("Graph pre-load", t2 - t1); }

    for (int64_t trial = 0; trial < args.numTrials; ++trial)
    {
        Hooks::getInstance().trial = trial;
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
                    Hooks::getInstance().region_begin("deletions");
                    deleteEdges(modified_after, g);
                    Hooks::getInstance().region_end("deletions");
                }
                synchronize(pg);
            }

            // Batch insertion
            if (process_id(pg) == 0) { cerr << "Loading batch " << batchId << "...\n"; }
            Hooks::getInstance().region_begin("insertions");
            insertBatch(dataset->getBatch(batchId), g, max_num_vertices);
            Hooks::getInstance().region_end("insertions");

            synchronize(pg);

            cout << "{\"pid\":"         << process_id(pg) << ","
                 << "\"num_vertices\":" << num_vertices(g) << ","
                 << "\"num_edges\":"    << num_edges(g) << "}\n";

            // Algorithm
            for (string algName : split(args.algName, ' '))
            {
                Hooks::getInstance().region_begin(algName);
                runAlgorithm(algName, g, max_num_vertices, trial);
                Hooks::getInstance().region_end(algName);
            }
        }
    }

    return 0;
}