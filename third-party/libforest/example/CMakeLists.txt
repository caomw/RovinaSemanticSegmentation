cmake_minimum_required(VERSION 2.6)
project(test)

set(CMAKE_CXX_FLAGS "-std=c++0x")

add_subdirectory(libforest)

file(GLOB SOURCES "*.cpp")

include_directories(../include)
message(STATUS ${LIBF_INCLUDE_DIRS})
add_executable(cli_mcmcf ${SOURCES})
target_link_libraries( cli_mcmcf libforest )
find_package(OpenMP)
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

