//
// Created by ehein6 on 8/3/16.
//

#include "boost_algs.h"
#include <dynograph_util.hh>

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

    if      (algName == "bc")  { run_bc(g, nv);  }
    else if (algName == "bfs") { run_bfs(g, nv); }
    else if (algName == "cc")  { run_cc(g, nv); }
    else if (algName == "gc")  { run_gc(g, nv); }
    else if (algName == "pagerank") { run_pagerank(g, nv); }
    else if (algName == "sssp"){ run_sssp(g, nv); }
    else
    {
        if (g.process_group().rank == 0) { cerr << "Algorithm " << algName << " not implemented!\n"; }
        exit(-1);
    }
}
