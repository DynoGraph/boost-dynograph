#include "../boost_algs.h"
#include <boost/graph/distributed/connected_components_parallel_search.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/graph/distributed/strong_components.hpp>

void run_cc(Graph &g)
{
    std::vector<int> local_components_vec(boost::num_vertices(g));
    boost::graph::distributed::fleischer_hendrickson_pinar_strong_components(
            g,
            make_iterator_property_map(local_components_vec.begin(), get(boost::vertex_index, g))
    );
}
