cmake_minimum_required(VERSION 3.10)
project(mini_path_tracer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(path_tracer src/main.cpp)
target_link_libraries(path_tracer PRIVATE Threads::Threads)