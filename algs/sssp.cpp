#include "../boost_algs.h"
#include <boost/graph/distributed/delta_stepping_shortest_paths.hpp>

void run_sssp(Graph &g, VertexId source)
{
    std::vector<int> local_distance_vec(boost::num_vertices(g));
    std::vector<VertexId> local_predecessor_vec(boost::num_vertices(g));
    boost::graph::distributed::delta_stepping_shortest_paths(
        g,
        source,
        make_iterator_property_map(local_predecessor_vec.begin(), get(boost::vertex_index, g)),
        make_iterator_property_map(local_distance_vec.begin(), get(boost::vertex_index, g)),
        get(boost::edge_weight, g)
    );
}
