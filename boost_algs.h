//
// Created by ehein6 on 8/3/16.
//

#ifndef BOOST_DYNOGRAPH_BOOST_ALGS_H
#define BOOST_DYNOGRAPH_BOOST_ALGS_H

// HACK This includes a modified version of the boost graph iteration macro header that counts edges
#include "iteration_macros.hpp"
#include "breadth_first_search.hpp"

#include "graph_config.h"
#include <string>
#include <inttypes.h>

void runAlgorithm(std::string algName, Graph &g, const std::vector<int64_t> &sources);

void run_bc(Graph &g);
void run_bfs(Graph &g, Vertex source);
void run_cc(Graph &g);
void run_gc(Graph &g);
void run_sssp(Graph &g, Vertex source);
void run_pagerank(Graph &g);


#endif //BOOST_DYNOGRAPH_BOOST_ALGS_H
