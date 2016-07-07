#include <map>
#include <iostream>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/edge_list.hpp>
#include <boost/graph/directed_graph.hpp>
#include <boost/graph/page_rank.hpp>
#include <boost/graph/betweenness_centrality.hpp>
#include <boost/property_map/property_map.hpp>
#include <fstream>
#include <chrono>


using std::cout;
using std::endl;
using std::ifstream;
using std::map;
using std::chrono::steady_clock;
using std::chrono::duration;

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS> vecGraph;

namespace std {
    template<typename T>
    std::istream &operator>>(std::istream &in, std::pair<T, T> &p) {
        in >> p.first >> p.second;
        return in;
    }
}

int main() {
    std::ifstream input("/ssd/sc15/sc15-10K.el");

    typedef boost::graph_traits<vecGraph>::vertices_size_type size_type;
    typedef boost::graph_traits<vecGraph>::vertex_descriptor vertex;

    std::map<vertex, double> mymap;
    boost::associative_property_map<std::map<vertex, double>> rank_map(mymap);

    size_type n_vertices = 10001;

    std::istream_iterator<std::pair<size_type, size_type>> input_begin(input), input_end;

    auto start = steady_clock::now();

    vecGraph g(input_begin, input_end, n_vertices);

    auto end = steady_clock::now();

    auto diff = end - start;

    cout << "Import done in " << duration<double, std::milli>(diff).count() << " milliseconds. " << endl;

    cout << "Number of vertices: " << g.m_vertices.size() << endl;
    cout << "Number of edges: " << g.m_edges.size() << endl;

    cout << "Running page rank." << endl;

    start = steady_clock::now();

    boost::graph::page_rank(g,  rank_map, boost::graph::n_iterations(20), 0.85);

    end = steady_clock::now();

    diff = end - start;

    cout << "Page rank done in " << duration<double, std::milli>(diff).count() << " milliseconds. " << endl;

    return 0;
}