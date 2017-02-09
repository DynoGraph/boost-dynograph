#include "boost_dynamic_graph.h"

int main(int argc, char *argv[]) {
    // Initialize MPI
    boost::mpi::environment env(argc, argv);
    // Run the benchmark
    DynoGraph::run<boost_dynamic_graph>(argc, argv);
    return 0;
}