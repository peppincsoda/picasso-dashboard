cmake_minimum_required(VERSION 3.7)

project(picasso-dashboard)

enable_testing()

set(MY_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

add_subdirectory(dashboard)
