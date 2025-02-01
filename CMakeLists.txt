cmake_minimum_required(VERSION 3.0)
project(Partial)

include_directories(
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/utility
)

set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -std=c++14 -O0 -g -Wall -march=native -pthread")
        # "${CMAKE_CXX_FLAGS} -std=c++14 -O0 -g -Wall -march=native -pthread -fsanitize=address")

add_subdirectory(utility)
add_subdirectory(graph)
add_subdirectory(matching)
