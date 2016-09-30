//
// Created by ehein6 on 8/3/16.
//

#ifndef BOOST_DYNOGRAPH_BOOST_ALGS_H
#define BOOST_DYNOGRAPH_BOOST_ALGS_H

// HACK This includes a modified version of the boost graph iteration macro header that counts edges
#include "iteration_macros.hpp"
#include "graph_config.h"
#include <string>
#include <inttypes.h>

void runAlgorithm(std::string algName, Graph &g, int64_t trial);

#endif //BOOST_DYNOGRAPH_BOOST_ALGS_H
