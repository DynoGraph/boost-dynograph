cmake_minimum_required (VERSION 2.8.12)
project (BOOST-DYNOGRAPH)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_SOURCE_DIR}/cmake")

include(cotire)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

# From /usr/local/include/boost/graph/adjacency_list.hpp
# vecS listS setS mapS multisetS multimapS hash_setS hash_mapS hash_multisetS hash_multimapS
# Stick with vecS for vertex list, otherwise there's no way to refer to a vertex by ID
# Everything except multimapS seems to work for the edge list type
set(OUT_EDGE_LIST_TYPE "vecS" CACHE STRING "Data structure used for the edge list")
set(VERTEX_LIST_TYPE   "vecS" CACHE STRING "Data structure used for the vertex list")

configure_file(
    ${CMAKE_SOURCE_DIR}/graph_config.h.in
    ${CMAKE_SOURCE_DIR}/graph_config.h
    @ONLY
)

# Find OpenMP
find_package(OpenMP REQUIRED)
if(OPENMP_FOUND)
 set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
 set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

# Find Boost and check version
find_package(Boost 1.61.0 REQUIRED
    COMPONENTS graph graph_parallel mpi serialization system)

# Find mpi
find_package(MPI)

# Build libraries
add_subdirectory(dynograph_util)
add_subdirectory(hooks)

# Build executable
add_executable(boost-dynograph main.cc distributed_dataset.cpp distributed_dataset.h boost_algs.cpp boost_algs.h)
target_include_directories(boost-dynograph PRIVATE
    ${CMAKE_SOURCE_DIR}/dynograph_util
    ${CMAKE_SOURCE_DIR}/hooks
    ${Boost_INCLUDE_DIRS}
    ${MPI_CXX_INCLUDE_PATH}
)
target_link_libraries(boost-dynograph
    dynograph_util
    hooks
    ${Boost_LIBRARIES}
    ${MPI_CXX_LIBRARIES}
)
# Enable precompiled headers
cotire(boost-dynograph)
