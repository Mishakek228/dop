cmake_minimum_required(VERSION 3.15)
project(ProjFSProvider)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(my_provider main.cpp)

target_link_libraries(my_provider PRIVATE projectedfslib)