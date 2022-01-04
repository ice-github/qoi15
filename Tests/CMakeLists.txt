cmake_minimum_required(VERSION 3.16)
project(qoi15test)

add_executable(qoi15test Test.cpp)
target_include_directories(qoi15test PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(qoi15test qoi15library GTest::GTest GTest::Main ${OpenCV_LIBS})
