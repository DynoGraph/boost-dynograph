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
#include <dynograph_util.hh>

using std::string;
using std::cerr;

VertexId
pickSource(Graph &g, Graph::vertices_size_type maxNumVertices)
{
    DynoGraph::VertexPicker picker(maxNumVertices, 0);
    VertexId source;
    Graph::degree_size_type degree;
    boost::mpi::communicator comm = communicator(process_group(g));
    do {
        int64_t source_id = picker.next();
        source = boost::vertex(source_id, g);
        if (source.owner == comm.rank()) {
            degree = boost::out_degree(source, g);
            boost::mpi::broadcast(comm, degree, source.owner);
        } else {
            boost::mpi::broadcast(comm, degree, source.owner);
        }
    }
    while (degree == 0);
    return source;
}

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

void runAlgorithm(string algName, Graph &g, Graph::vertices_size_type maxNumVertices)
{
    if (g.process_group().rank == 0) { cerr << "Running " << algName << "...\n"; }

    if (algName == "bc")
    {
        std::vector<int64_t> local_centrality_vec(boost::num_vertices(g), 0);
        boost::brandes_betweenness_centrality(
            g,
            make_iterator_property_map(local_centrality_vec.begin(), get(boost::vertex_index, g))
        );
    }

    else if (algName == "bfs")
    {
        VertexId source = pickSource(g, maxNumVertices);
        std::vector<int> local_distance_vec(boost::num_vertices(g));
        auto distance_map = make_iterator_property_map(local_distance_vec.begin(), get(boost::vertex_index, g));
        bfs_discovery_visitor<decltype(distance_map)> visitor(distance_map);
        breadth_first_search(
            g,
            source,
            boost::visitor(visitor)
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
        VertexId source = pickSource(g, maxNumVertices);
        boost::graph::distributed::delta_stepping_shortest_paths(
            g,
            source,
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
