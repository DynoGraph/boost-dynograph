cmake_minimum_required (VERSION 2.8.12)
project (BOOST-DYNOGRAPH)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_SOURCE_DIR}/cmake")

# Cotire speeds up the build by using precompiled headers
# But it can't figure out the hack that replaces iteration_macros.hpp
# So we can't use it anymore
#include(cotire)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

# From /usr/local/include/boost/graph/adjacency_list.hpp
# vecS listS setS mapS multisetS multimapS hash_setS hash_mapS hash_multisetS hash_multimapS
# Stick with vecS for the vertex list, otherwise there's no way to refer to a vertex by ID
# Everything except multimapS seems to work for the edge list type in serial mode
# Distributed graphs only support vecS and listS
set(OUT_EDGE_LIST_TYPE "vecS" CACHE STRING "Data structure used for the edge list")
set(VERTEX_LIST_TYPE   "vecS" CACHE STRING "Data structure used for the vertex list")

configure_file(
    ${CMAKE_SOURCE_DIR}/graph_config.h.in
    ${CMAKE_SOURCE_DIR}/graph_config.h
    @ONLY
)

# Find MPI installation
find_package(MPI)

# Find Boost and check version
find_package(Boost 1.61.0 REQUIRED
    COMPONENTS graph graph_parallel mpi serialization system)

include_directories(
    ${Boost_INCLUDE_DIRS}
    ${MPI_CXX_INCLUDE_PATH}
)
link_libraries(
    ${Boost_LIBRARIES}
    ${MPI_CXX_LIBRARIES}
)

# Build dynograph_util and hooks in MPI mode
add_definitions(-DUSE_MPI)
add_subdirectory(dynograph_util)
include_directories(
    ${CMAKE_SOURCE_DIR}/dynograph_util
    ${CMAKE_SOURCE_DIR}/dynograph_util/hooks
)
link_libraries(
    dynograph_util
    hooks
)

# Build executable
add_executable(boost-dynograph
    main.cc
    boost_dynamic_graph.cpp
    boost_algs.cpp
    algs/bc.cpp
    algs/bfs.cpp
    algs/cc.cpp
    algs/gc.cpp
    algs/pagerank.cpp
    algs/sssp.cpp
)

# Enable precompiled headers
#cotire(boost-dynograph)

# Build tests
add_executable(boost_dynograph_test
    boost_dynograph_test.cpp
    boost_dynamic_graph.cpp
    boost_algs.cpp
    algs/bc.cpp
    algs/bfs.cpp
    algs/cc.cpp
    algs/gc.cpp
    algs/pagerank.cpp
    algs/sssp.cpp
)
target_link_libraries(boost_dynograph_test gtest_main)
enable_testing()
add_test(boost_dynograph_test boost_dynograph_test)
find_program(MPIRUN mpirun REQUIRED)
add_test(
    NAME mpi_boost_dynograph_test
    COMMAND mpirun -np 4 boost_dynograph_test
)
