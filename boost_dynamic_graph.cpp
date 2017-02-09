
#include "boost_dynamic_graph.h"
#include "boost_algs.h"
#include <chrono>
#include <sstream>
#include <vector>

boost_dynamic_graph::boost_dynamic_graph(DynoGraph::Args args, int64_t max_vertex_id)
: DynoGraph::DynamicGraph(args, max_vertex_id)
, g(static_cast<BoostVertexId>(max_vertex_id+1))
, global_max_nv(max_vertex_id+1)
{}

void
boost_dynamic_graph::before_batch(const DynoGraph::Batch &batch, int64_t threshold) {

}

void
boost_dynamic_graph::delete_edges_older_than(int64_t threshold) {
    boost::remove_edge_if(
            [&](const BoostEdge &e)
            {
                return get(boost::edge_timestamp, g, e) < threshold;
            }
            ,g);
    synchronize(g);
}

void
boost_dynamic_graph::insert_batch(const DynoGraph::Batch &batch) {
    for (DynoGraph::Edge e : batch)
    {
        assert(e.src > 0 && e.dst > 0);
        assert(static_cast<Graph::vertices_size_type>(e.src) < global_max_nv);
        assert(static_cast<Graph::vertices_size_type>(e.dst) < global_max_nv);
        Hooks::getInstance().traverse_edges(1);
        BoostVertex Src = boost::vertex(e.src, g);
        BoostVertex Dst = boost::vertex(e.dst, g);
        // Try to insert the edge
        std::pair<BoostEdge, bool> inserted_edge = boost::add_edge(
                Src, Dst,
                // Boost uses template nesting to implement multiple edge properties
                Weight(e.weight, Timestamp(e.timestamp)),
                g);
        // If the edge already existed...
        if (!inserted_edge.second) {
            BoostEdge &existing_edge = inserted_edge.first;
            // Increment edge weight
            int64_t weight = get(boost::edge_weight, g, existing_edge);
            weight += e.weight;
            put(boost::edge_weight, g, existing_edge, weight);
            // Overwrite timestamp
            put(boost::edge_timestamp, g, existing_edge, e.timestamp);
        }
    }
    synchronize(g);
}

void
boost_dynamic_graph::update_alg(const std::string &alg_name, const std::vector<int64_t> &sources) {
    runAlgorithm(alg_name, g, sources);
}

int64_t
boost_dynamic_graph::get_out_degree(int64_t vertex_id) const {
    BoostVertex v = boost::vertex(vertex_id, g);
    if (owner(v) == process_group(g).rank) {
        return boost::out_degree(v, g);
    } else {
        return 0;
    }
}

int64_t
boost_dynamic_graph::get_num_vertices() const {
    return num_vertices(g);
}

int64_t
boost_dynamic_graph::get_num_edges() const {
    return num_edges(g);
}

std::vector<std::string> get_supported_algs() { return {"bfs", "cc", "gc", "sssp", "pagerank"}; }
