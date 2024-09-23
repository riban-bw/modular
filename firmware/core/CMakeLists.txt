cmake_minimum_required(VERSION 3.11)
project(modular VERSION 0.1)
add_executable(modular)
#set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_CXX_STANDARD_REQUIRED True)
target_include_directories(modular PUBLIC include)
target_sources(modular PRIVATE
    src/main.cpp
    src/can.cpp)