cmake_minimum_required (VERSION 2.8.12)
project (BOOST-DYNOGRAPH)
include (ExternalProject)
include (ProcessorCount)
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
find_package(MPI REQUIRED)

# Find boost libraries
set(Boost_components graph graph_parallel mpi serialization system)
find_package(Boost 1.61.0 COMPONENTS ${Boost_components})
if (NOT ${Boost_FOUND})
    # Detect processor count
    ProcessorCount(NPROCS)
    if (NPROCS EQUAL 0)
        set(NPROCS 2)
    endif()
    # Download, build, and install boost libraries
    ExternalProject_Add(Boost
        PREFIX                  ${CMAKE_BINARY_DIR}/external
        URL                     https://sourceforge.net/projects/boost/files/boost/1.61.0/boost_1_61_0.tar.gz
        URL_HASH                SHA256=a77c7cc660ec02704c6884fbb20c552d52d60a18f26573c9cee0788bf00ed7e6
        CONFIGURE_COMMAND       ./bootstrap.sh
                  COMMAND       echo "using mpi $<SEMICOLON>" >> ./project-config.jam
        BUILD_COMMAND           ./b2 -j${NPROCS} --with-graph --with-graph_parallel --with-mpi --with-serialization --with-system
        BUILD_IN_SOURCE         TRUE
        INSTALL_COMMAND         ./b2 install -j${NPROCS} --with-graph --with-graph_parallel --with-mpi --with-serialization --with-system --prefix=${CMAKE_BINARY_DIR}/external
    )
    ExternalProject_Get_Property(Boost INSTALL_DIR)
    set(Boost_INCLUDE_DIRS ${INSTALL_DIR}/include)
    foreach(component ${Boost_components})
        add_library(boost_${component} SHARED IMPORTED)
        add_dependencies(boost_${component} Boost)
        set_target_properties(boost_${component} PROPERTIES
            IMPORTED_LOCATION ${INSTALL_DIR}/lib/libboost_${component}.so)
        list(APPEND Boost_LIBRARIES boost_${component})
    endforeach()
endif()

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
