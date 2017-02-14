
#include "boost_dynamic_graph.h"
#include "boost_algs.h"
#include <chrono>
#include <sstream>
#include <vector>
#include <functional>
#include <tuple>
#include <hooks/dynograph_edge_count.h>

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

class edge_update : public DynoGraph::Edge
{
private:
    bool done;
public:
    edge_update() = default;
    edge_update(const DynoGraph::Edge &e) : DynoGraph::Edge(e), done(false) {}
    void mark_done() { done = true; }
    bool is_done() const { return done; }
    friend bool operator<(const edge_update& a, const edge_update& b);
};
bool
operator<(const edge_update& a, const edge_update& b)
{
    return std::tie(a.src, a.dst, a.weight, a.timestamp, a.done)
           < std::tie(b.src, b.dst, b.weight, b.timestamp, b.done);
}
BOOST_IS_BITWISE_SERIALIZABLE(class edge_update);

// Finds an element in a sorted range using binary search
// http://stackoverflow.com/a/446327/1877086
template<class Iter, class T, class Compare>
static Iter
binary_find(Iter begin, Iter end, T val, Compare comp)
{
    // Finds the lower bound in at most log(last - first) + 1 comparisons
    Iter i = std::lower_bound(begin, end, val, comp);

    if (i != end && !(comp(val, *i)))
        return i; // found
    else
        return end; // not found
}

void
boost_dynamic_graph::insert_batch(const DynoGraph::Batch &batch)
{
    // 1. Distribute the updates to the rank that will perform them
    using std::vector;
    auto comm = boost::mpi::communicator();
    const int num_ranks = comm.size();
    vector<edge_update> local_updates;
    if (comm.rank() == 0)
    {
        // Copy the batch to a local list
        vector<edge_update> global_updates(batch.begin(), batch.end());
        // Sort the list of updates by the rank that owns the source vertex
        auto by_mapping = [this](const edge_update &a, const edge_update &b) {
            BoostVertex va = boost::vertex(a.src, g);
            BoostVertex vb = boost::vertex(b.src, g);
            return va.owner < vb.owner;
        };
        std::sort(global_updates.begin(), global_updates.end(), by_mapping);

        // Scan the list to calculate the number of updates for each rank
        vector<int> sizes_by_rank(num_ranks, 0);
        for (auto iter = global_updates.begin(); iter < global_updates.end(); ++iter)
        {
            // Owner of current range
            int current_rank = boost::vertex(iter->src, g).owner;
            // Start of range
            auto prev = iter;
            // Find the position of the first update that does not belong to this rank
            iter = std::upper_bound(global_updates.begin(), global_updates.end(), *iter, by_mapping);
            // Compute length of range
            sizes_by_rank[current_rank] = iter - prev;
        }

        // Send number of updates for each rank
        int local_num_updates;
        boost::mpi::scatter(comm, sizes_by_rank, local_num_updates, 0);
        // Allocate buffer
        local_updates.resize(local_num_updates);
        // Scatter the updates to the proper rank
        boost::mpi::scatterv(comm, global_updates.data(), sizes_by_rank, local_updates.data(), 0);

    } else {
        // Receive number of updates for this rank
        size_t local_num_updates;
        boost::mpi::scatter(comm, local_num_updates, 0);
        // Allocate buffer
        local_updates.resize(local_num_updates);
        // Receive list of local updates
        boost::mpi::scatterv(comm, local_updates.data(), local_num_updates, 0);
    }

    // TODO implement bulk load in snapshot mode

    // 2. Update existing edges
    std::sort(local_updates.begin(), local_updates.end());
    auto same_src_and_dst = [](const edge_update& a, const edge_update& b) { return a.src == b.src && a.dst == b.dst; };
    // TODO Filter by source vertex to reduce search time
    BGL_FORALL_EDGES_T(e, g, decltype(g))
    {
        // Make sure we are updating local vertices
        assert(e.source_owns_edge && e.source_processor == comm.rank());

        auto src_dst_compare = [](const edge_update& a, const edge_update& b) {
            return std::tie(a.src, a.dst) < std::tie(b.src, b.dst);
        };
        // Find and perform updates that match this edge
        edge_update key;
        key.src = e.local.m_source;
        key.dst = e.local.m_target;
        auto weight = get(boost::edge_weight, g, e);
        auto timestamp = get(boost::edge_timestamp, g, e);
        // Use binary search to find the first matching update
        for (auto u = binary_find(local_updates.begin(), local_updates.end(), key, src_dst_compare);
        // Keep walking the list until we reach the last update for this edge
            u < local_updates.end() && !src_dst_compare(key, *u); ++u)
        {
            // Increment edge weight
            weight += u->weight;
            // Update timestamp
            timestamp = std::max(timestamp, u->timestamp);
            // Mark this update as done
            u->mark_done();
        }
        put(boost::edge_weight, g, e, weight);
        put(boost::edge_timestamp, g, e, timestamp);
    }

    // 3. Add any remaining updates to the graph as new edges
    for (auto u = local_updates.begin(); u < local_updates.end();)
    {
        // Skip past updates that were processed in step 2
        if (u->is_done()) {
            ++u;
            continue;
        }

        // Make sure we are using valid vertices
        assert(u->src > 0 && u->dst > 0);
        assert(static_cast<Graph::vertices_size_type>(u->src) < global_max_nv);
        assert(static_cast<Graph::vertices_size_type>(u->dst) < global_max_nv);
        BoostVertex Src = boost::vertex(u->src, g);
        BoostVertex Dst = boost::vertex(u->dst, g);

        // Combine properties from this and all duplicates of this edge
        int64_t weight = 0;
        int64_t timestamp = 0;
        for (auto first = u; u < local_updates.end() && same_src_and_dst(*first, *u); ++u)
        {
            weight += u->weight;
            timestamp = std::max(timestamp, u->timestamp);
            u->mark_done();
        }

        // Insert the edge
        boost::add_edge(
            Src, Dst,
            // Boost uses template nesting to implement multiple edge properties
            Weight(weight, Timestamp(timestamp)),
            g
        );
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
    auto comm = communicator(g.process_group());
    int64_t nv = 0;
    BGL_FORALL_VERTICES_T(v, g, decltype(g))
    {
        if (boost::out_degree(v, g) > 0) { nv += 1; }
    }
    boost::mpi::all_reduce(comm, nv, std::plus<decltype(nv)>());
    return nv;
}

int64_t
boost_dynamic_graph::get_num_edges() const {
    auto comm = communicator(g.process_group());
    auto ne = num_edges(g);
    boost::mpi::all_reduce(comm, ne, std::plus<decltype(ne)>());
    return ne;
}

std::vector<std::string>
boost_dynamic_graph::get_supported_algs() {
    return {"bfs", "cc", "gc", "sssp", "pagerank"};
}

void
boost_dynamic_graph::dump()
{
//    auto local_vertices = vertices(g);
//    for (auto vi = local_vertices.first; vi != local_vertices.second; ++vi) {
//        BoostVertex v = *vi;
//        auto local_out_edges = out_edges(v, g);
//        for (auto ei = local_out_edges.first; ei != local_out_edges.second; ++ei) {
//            BoostEdge e = *ei;
//            auto weight = get(boost::edge_weight, g, e);
//            auto ts = get(boost::edge_timestamp, g, e);
//            std::cout << e.local.m_source << " " << e.local.m_target << " " << weight << " " << ts << " " << "\n";
//        }
//    }

    BGL_FORALL_EDGES_T(e, g, decltype(g))
    {
        auto weight = get(boost::edge_weight, g, e);
        auto ts = get(boost::edge_timestamp, g, e);
        std::cout << e.local.m_source << " " << e.local.m_target << " " << weight << " " << ts << " " << "\n";
    }
}