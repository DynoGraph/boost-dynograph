#include "../boost_algs.h"
#include <boost/graph/distributed/betweenness_centrality.hpp>

void run_bc(Graph &g)
{
    std::vector<int64_t> local_centrality_vec(boost::num_vertices(g), 0);
    boost::brandes_betweenness_centrality(
            g,
            make_iterator_property_map(local_centrality_vec.begin(), get(boost::vertex_index, g))
    );
}
