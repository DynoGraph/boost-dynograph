cmake_minimum_required (VERSION 2.8.12)
project (BOOST-DYNOGRAPH)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")


#Build with OpenMP
find_package(OpenMP REQUIRED)
if(OPENMP_FOUND)
 set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
 set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
 set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

add_subdirectory(dynograph_util)
add_subdirectory(hooks)

add_executable(boost-dynograph main.cc)

find_package(Boost 1.57.0 REQUIRED)

target_include_directories(boost-dynograph PRIVATE
    ${CMAKE_SOURCE_DIR}/dynograph_util
    ${CMAKE_SOURCE_DIR}/hooks
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(boost-dynograph
    dynograph_util
    hooks
)

