#pragma once
#include <dynograph_util.h>
#include "boost_algs.h"
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
    static std::vector<std::string> get_supported_algs();

    void dump();
};