#include "../boost_algs.h"
// boost_algs already includes the modified version of bfs header
//#include "../breadth_first_search.hpp"

template<typename DistanceMap>
struct bfs_discovery_visitor : boost::bfs_visitor<>
{
    bfs_discovery_visitor(DistanceMap distance) : distance(distance)
    {
        boost::set_property_map_role(boost::vertex_distance, distance);
    }

    template<typename Edge, typename Graph>
    void tree_edge(Edge e, const Graph& g)
    {
        std::size_t new_distance = get(distance, boost::source(e, g)) + 1;
        put(distance, boost::target(e, g), new_distance);
    }

private:
    DistanceMap distance;
};

void run_bfs(Graph &g, VertexId source)
{
    std::vector<int> local_distance_vec(boost::num_vertices(g));
    auto distance_map = make_iterator_property_map(local_distance_vec.begin(), get(boost::vertex_index, g));
    bfs_discovery_visitor<decltype(distance_map)> visitor(distance_map);
    breadth_first_search(
            g,
            source,
            boost::visitor(visitor)
    );
}