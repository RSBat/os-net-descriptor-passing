cmake_minimum_required(VERSION 3.10)
project(os_net_descriptor_passing)

set(CMAKE_CXX_STANDARD 14)

add_executable(client client.cpp)
add_executable(server server.cpp)
