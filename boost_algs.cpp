//
// Created by ehein6 on 8/3/16.
//

#include "boost_algs.h"

#include <boost/graph/distributed/page_rank.hpp>
#include <boost/graph/distributed/betweenness_centrality.hpp>
#include <boost/graph/distributed/connected_components_parallel_search.hpp>

using std::string;
using std::cerr;

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
