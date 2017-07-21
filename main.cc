#include "boost_dynamic_graph.h"

int main(int argc, char *argv[]) {
    // Initialize MPI
    boost::mpi::environment env(argc, argv);
    // Run the benchmark
    DynoGraph::Benchmark::run<boost_dynamic_graph>(argc, argv);
    return 0;
}

/*
Notes

 I want to reduce the amount of MPI code that needs to go in dynograph_util
 Every object and function should either execute the same way in all processes,
 or be empty and put everything in rank 0.

 Args: can be parsed the same way in all processes
 Dataset: load in rank 0, empty in all others
    getMaxVertexID still has to be right (do in rank 0 and scatter)
    getBatch needs to return an empty batch
    getTimestampForWindow needs to be right (do in rank 0 and scatter)
 Logger: Singleton, aggregate messages to be printed in rank 0

 Hooks: Use existing MPI aggregation code


*/