#include <map>
#include <iostream>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/edge_list.hpp>
#include <boost/graph/directed_graph.hpp>
#include <boost/graph/page_rank.hpp>
#include <boost/graph/betweenness_centrality.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/program_options.hpp>

#include <fstream>
#include <chrono>

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

typedef boost::vecS OutEdgeList;
typedef boost::vecS VertexList;
// TODO add graph properties
typedef boost::adjacency_list<OutEdgeList, VertexList, boost::bidirectionalS> Graph;
typedef boost::graph_traits<Graph>::vertex_descriptor VertexId;

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


int main(int argc, char *argv[]) {
    // Parse the command line
    Args args = getArgs(argc, argv);

    // Create the graph
    // TODO scale this up with available system memory
    VertexId max_num_vertices = 10001;
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
            cerr << "Loading batch " << batchId << "...\n";
            t1 = steady_clock::now();
            hooks_region_begin(trial);

            // Batch insertion
            for (DynoGraph::Edge e : dataset.getBatch(batchId))
            {
                // TODO check return value and update weight&timestamp if present
                add_edge(e.src, e.dst, g);
            }

            hooks_region_end(trial);
            t2 = steady_clock::now();
            printTime("Edge stream", t2 - t1);

            cerr << "Number of vertices: " << g.m_vertices.size() << "\n";
            cerr << "Number of edges: " << g.m_edges.size() << "\n";

            cerr << "Running page rank...\n";
            t1 = steady_clock::now();
            hooks_region_begin(trial);

            // Algorithm
            std::map<VertexId, double> mymap;
            boost::associative_property_map<std::map<VertexId, double>> rank_map(mymap);
            boost::graph::page_rank(g,  rank_map, boost::graph::n_iterations(20), 0.85);

            hooks_region_end(trial);
            t2 = steady_clock::now();
            printTime("Page rank", t2 - t1);
        }
    }

    return 0;
}