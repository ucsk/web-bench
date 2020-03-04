cmake_minimum_required(VERSION 3.15)
project(WebBench C)

set(CMAKE_C_STANDARD 99)

add_executable(WebBench webbench.c)
target_sources(WebBench PRIVATE socket.c)
