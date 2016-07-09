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

using std::cerr;
using std::cerr;
using std::ifstream;
using std::map;
using std::chrono::steady_clock;
using std::chrono::duration;
using std::chrono::steady_clock;
using std::string;

namespace po = boost::program_options;

typedef boost::vecS OutEdgeList;
typedef boost::vecS VertexList;
// TODO add graph properties
typedef boost::adjacency_list<OutEdgeList, VertexList, boost::bidirectionalS> Graph;
typedef boost::graph_traits<Graph>::vertex_descriptor VertexId;

void printTime(string stepDesc, duration<double, std::milli> diff)
{
    cerr << stepDesc << ": " << diff.count() << " ms.\n";
}

int main(int argc, char *argv[]) {
    // Parse the command line
    std::string inputPath;
    po::options_description desc("Usage: ");
    desc.add_options()
        ("help", "Display help")
        ("input", po::value<std::string>(&inputPath), "Path for the graph to load")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help") || !vm.count("input"))
    {
        cerr << desc << "\n";
        return -1;
    }
  
    // Create the graph
    // TODO scale this up with available system memory
    VertexId max_num_vertices = 10001;
    Graph g(max_num_vertices);

    // Pre-load the edge batches
    cerr << "Pre-loading " << inputPath << " from disk...\n";
    auto t1 = steady_clock::now();

    DynoGraph::Dataset dataset(inputPath, 20);

    auto t2 = steady_clock::now();
    printTime("Graph pre-load", t2 - t1);

    // Ingest each batch and run analytics
    for (int batchId = 0; batchId < dataset.getNumBatches(); ++batchId)
    {
        cerr << "Loading batch " << batchId < "...\n";
        t1 = steady_clock::now();

        // Batch insertion
        for (DynoGraph::Edge e : dataset.getBatch(batchId))
        {   
            // TODO check return value and update weight&timestamp if present
            add_edge(e.src, e.dst, g);
        }

        t2 = steady_clock::now();
        printTime("Edge stream", t2 - t1);

        cerr << "Number of vertices: " << g.m_vertices.size() << "\n";
        cerr << "Number of edges: " << g.m_edges.size() << "\n";

        cerr << "Running page rank...\n";
        t1 = steady_clock::now();

        // Algorithm
        std::map<VertexId, double> mymap;
        boost::associative_property_map<std::map<VertexId, double>> rank_map(mymap);
        boost::graph::page_rank(g,  rank_map, boost::graph::n_iterations(20), 0.85);

        t2 = steady_clock::now();
        printTime("Page rank", t2 - t1);
    }

    return 0;
}