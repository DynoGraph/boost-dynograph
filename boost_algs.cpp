//
// Created by ehein6 on 8/3/16.
//

#include "boost_algs.h"
#include <dynograph_util.hh>
#include <hooks.h>

using std::string;
using std::cerr;

Vertex
pickSource(Graph &g)
{
    // Find the vertex with the highest degree in this process
    VertexId num = 1;
    VertexId nv = num_vertices(g);
    auto get_degree = [&g](VertexId i){
        Vertex v = boost::vertex(i, g);
        return boost::out_degree(v, g);
    };
    VertexId local_source = DynoGraph::find_high_degree_vertices(num, nv, get_degree)[0];

    // Reduce to get the highest degree across all processes
    auto compare = [get_degree](VertexId a, VertexId b) {
        int64_t degree_a = get_degree(a);
        int64_t degree_b = get_degree(b);
        if (degree_a != degree_b) { return degree_a < degree_b; }
        return a > b;
    };
    VertexId global_source;
    boost::mpi::communicator comm = communicator(process_group(g));
    boost::mpi::reduce(comm, local_source, global_source, compare, 0);

    // Broadcast to all processes
    boost::mpi::broadcast(comm, global_source, 0);
    return boost::vertex(global_source, g);
}

void runAlgorithm(string algName, Graph &g)
{
    if (g.process_group().rank == 0) { cerr << "Running " << algName << "...\n"; }

    Vertex source;
    if (algName == "bfs" || algName == "sssp")
    {
        source = pickSource(g);
    }

    Hooks& hooks = Hooks::getInstance();
    hooks.region_begin(algName);
    if      (algName == "bc")  { run_bc(g);  }
    else if (algName == "bfs") { run_bfs(g, source); }
    else if (algName == "cc")  { run_cc(g); }
    else if (algName == "gc")  { run_gc(g); }
    else if (algName == "pagerank") { run_pagerank(g); }
    else if (algName == "sssp"){ run_sssp(g, source); }
    else
    {
        if (g.process_group().rank == 0) { cerr << "Algorithm " << algName << " not implemented!\n"; }
        exit(-1);
    }
    synchronize(g);
    hooks.region_end();
}
