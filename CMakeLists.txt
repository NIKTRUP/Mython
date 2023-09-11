cmake_minimum_required(VERSION 3.21)
project(Mython)

file(GLOB source ./src/*.cpp)
file(GLOB includes ./include/*.h)

set(CMAKE_CXX_STANDARD 17)

add_executable(Mython main.cpp ${source} ${includes})
