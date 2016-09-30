//
// Created by ehein6 on 8/3/16.
//

#include "boost_algs.h"
#include <boost/graph/distributed/page_rank.hpp>
#include <boost/graph/distributed/betweenness_centrality.hpp>
#include <boost/graph/distributed/connected_components_parallel_search.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/graph/distributed/strong_components.hpp>
#include <boost/graph/distributed/delta_stepping_shortest_paths.hpp>
#include <boost/graph/distributed/boman_et_al_graph_coloring.hpp>

using std::string;
using std::cerr;

//std::vector<VertexId> bfsRoots = {3, 30, 300, 4, 40, 400};
std::vector<string> algs = {"all", "bc", "cc", "gc", "sssp", "pagerank"};

void runAlgorithm(string algName, Graph &g, int64_t trial)
{
    if (g.process_group().rank == 0) { cerr << "Running " << algName << "...\n"; }

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
        std::vector<int> local_components_vec(boost::num_vertices(g));
        boost::graph::distributed::fleischer_hendrickson_pinar_strong_components(
            g,
            make_iterator_property_map(local_components_vec.begin(), get(boost::vertex_index, g))
        );
    }

    else if (algName == "gc")
    {
        std::vector<int> local_color_vec(boost::num_vertices(g));
        boost::graph::boman_et_al_graph_coloring(
            g,
            make_iterator_property_map(local_color_vec.begin(), get(boost::vertex_index, g))
        );
    }

    else if (algName == "sssp")
    {
        std::vector<int> local_distance_vec(boost::num_vertices(g));
        std::vector<VertexId> local_predecessor_vec(boost::num_vertices(g));
        boost::graph::distributed::delta_stepping_shortest_paths(
            g,
            vertex(0, g), // TODO pick deterministic random vertex
            make_iterator_property_map(local_predecessor_vec.begin(), get(boost::vertex_index, g)),
            make_iterator_property_map(local_distance_vec.begin(), get(boost::vertex_index, g)),
            get(boost::edge_weight, g)
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
        if (g.process_group().rank == 0) { cerr << "Algorithm " << algName << " not implemented!\n"; }
        exit(-1);
    }
}