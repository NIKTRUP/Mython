cmake_minimum_required(VERSION 3.24)
project(Mython)

file(GLOB source *.cpp *.h)

set(CMAKE_CXX_STANDARD 17)

add_executable(Mython ${source})
