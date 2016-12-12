#include "boost_algs.h"

#include <dynograph_util.h>
#include "distributed_dataset.h"

#include <chrono>
#include <sstream>
#include <vector>

class boost_dynamic_graph : public DynoGraph::DynamicGraph
{
protected:
    Graph g;
public:
    boost_dynamic_graph(const DynoGraph::Dataset &dataset, const DynoGraph::Args &args)
    : DynoGraph::DynamicGraph(dataset, args)
    , g(static_cast<VertexId>(dataset.getMaxNumVertices()))
    {}

    virtual void before_batch(const DynoGraph::Batch &batch, int64_t threshold) override {

    }

    virtual void delete_edges_older_than(int64_t threshold) override {
        boost::remove_edge_if(
            [&](const Edge &e)
            {
                return get(boost::edge_timestamp, g, e) < threshold;
            }
            ,g);
        synchronize(g);
    }

    virtual void insert_batch(const DynoGraph::Batch &batch) override {
        VertexId max_nv = dataset.getMaxNumVertices();
        for (DynoGraph::Edge e : batch)
        {
            assert(e.src > 0 && e.dst > 0);
            assert(static_cast<Graph::vertices_size_type>(e.src) < max_nv);
            assert(static_cast<Graph::vertices_size_type>(e.dst) < max_nv);
            Hooks::getInstance().traverse_edges(1);
            Vertex Src = boost::vertex(e.src, g);
            Vertex Dst = boost::vertex(e.dst, g);
            // Try to insert the edge
            std::pair<Edge, bool> inserted_edge = boost::add_edge(
                Src, Dst,
                // Boost uses template nesting to implement multiple edge properties
                Weight(e.weight, Timestamp(e.timestamp)),
                g);
            // If the edge already existed...
            if (!inserted_edge.second) {
                Edge &existing_edge = inserted_edge.first;
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

    virtual void update_alg(const std::string &alg_name, const std::vector<int64_t> &sources) override {
        runAlgorithm(alg_name, g, sources);
    }

    virtual int64_t get_out_degree(int64_t vertex_id) const override {
        Vertex v = boost::vertex(vertex_id, g);
        if (owner(v) == process_group(g).rank) {
            return boost::out_degree(v, g);
        } else {
            return 0;
        }
    }

    virtual int64_t get_num_vertices() const override {
        return num_vertices(g);
    }

    virtual int64_t get_num_edges() const override {
        return num_edges(g);
    }

};

int main(int argc, char *argv[]) {
    // Initialize MPI
    boost::mpi::environment env(argc, argv);
    // Run the benchmark
    DynoGraph::run<boost_dynamic_graph>(argc, argv);
    return 0;
}