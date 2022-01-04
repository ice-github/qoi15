cmake_minimum_required(VERSION 3.16)
project(qoi15)
set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)
find_package(GTest REQUIRED)

add_subdirectory(Libraries)
add_subdirectory(Tests)
