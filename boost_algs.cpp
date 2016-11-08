//
// Created by ehein6 on 8/3/16.
//

#include "boost_algs.h"
#include <dynograph_util.hh>
#include <hooks.h>

using std::string;
using std::cerr;

VertexId
pickSource(Graph &g, Graph::vertices_size_type nv)
{
    DynoGraph::VertexPicker picker(nv, 0);
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

void runAlgorithm(string algName, Graph &g, Graph::vertices_size_type nv)
{
    if (g.process_group().rank == 0) { cerr << "Running " << algName << "...\n"; }

    VertexId source;
    if (algName == "bfs" || algName == "sssp")
    {
        source = pickSource(g, nv);
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
    hooks.region_end();
}
