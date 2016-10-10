#include "../boost_algs.h"
#include <boost/graph/distributed/page_rank.hpp>

void run_pagerank(Graph &g)
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
