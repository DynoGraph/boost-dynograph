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

VertexId pickSource(Graph &g, Graph::vertices_size_type nv);
void runAlgorithm(std::string algName, Graph &g, Graph::vertices_size_type nv);

void run_bc(Graph &g, Graph::vertices_size_type nv);
void run_bfs(Graph &g, Graph::vertices_size_type nv);
void run_cc(Graph &g, Graph::vertices_size_type nv);
void run_gc(Graph &g, Graph::vertices_size_type nv);
void run_sssp(Graph &g, Graph::vertices_size_type nv);
void run_pagerank(Graph &g, Graph::vertices_size_type nv);


#endif //BOOST_DYNOGRAPH_BOOST_ALGS_H
