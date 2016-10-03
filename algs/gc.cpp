#include "../boost_algs.h"
#include <boost/graph/distributed/boman_et_al_graph_coloring.hpp>

void run_gc(Graph &g, Graph::vertices_size_type nv)
{
    std::vector<int> local_color_vec(boost::num_vertices(g));
    boost::graph::boman_et_al_graph_coloring(
            g,
            make_iterator_property_map(local_color_vec.begin(), get(boost::vertex_index, g))
    );
}
