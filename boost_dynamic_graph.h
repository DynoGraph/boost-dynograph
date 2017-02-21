#pragma once
#include <dynograph_util.h>
#include "boost_algs.h"

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
bool operator<(const edge_update& a, const edge_update& b);
BOOST_IS_BITWISE_SERIALIZABLE(class edge_update);

class boost_dynamic_graph : public DynoGraph::DynamicGraph
{
protected:
    Graph g;
    BoostVertexId global_max_nv;
public:
    boost_dynamic_graph(DynoGraph::Args args, int64_t max_vertex_id);

    virtual void before_batch(const DynoGraph::Batch &batch, int64_t threshold) override;
    virtual void delete_edges_older_than(int64_t threshold) override;
    virtual void insert_batch(const DynoGraph::Batch &batch) override;
    virtual void update_alg(const std::string &alg_name, const std::vector<int64_t> &sources);
    virtual int64_t get_out_degree(int64_t vertex_id) const override;
    virtual int64_t get_num_vertices() const override;
    virtual int64_t get_num_edges() const override;
    virtual std::vector<int64_t> get_high_degree_vertices(int64_t n) const override;
    static std::vector<std::string> get_supported_algs();

    void dump() const;

    static void scatter_batch(const Graph& g, const DynoGraph::Batch &batch, std::vector<edge_update> &local_updates);

};

