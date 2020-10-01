cmake_minimum_required(VERSION 3.15)
project(web-bench C)

set(CMAKE_C_STANDARD 90)

add_executable(web-bench web-bench.c)
target_sources(web-bench PRIVATE socket.c)
