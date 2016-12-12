//
// Created by ehein6 on 8/3/16.
//

#include "boost_algs.h"

using std::string;
using std::cerr;

void runAlgorithm(string algName, Graph &g, const std::vector<int64_t> &sources)
{
    if      (algName == "bc")  { run_bc(g);  }
    else if (algName == "bfs") { run_bfs(g, boost::vertex(sources[0], g)); }
    else if (algName == "cc")  { run_cc(g); }
    else if (algName == "gc")  { run_gc(g); }
    else if (algName == "pagerank") { run_pagerank(g); }
    else if (algName == "sssp"){ run_sssp(g, boost::vertex(sources[0], g)); }
    else
    {
        if (g.process_group().rank == 0) { cerr << "Algorithm " << algName << " not implemented!\n"; }
        exit(-1);
    }
    synchronize(g);
}
