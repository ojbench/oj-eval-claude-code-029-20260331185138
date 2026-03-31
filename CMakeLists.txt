cmake_minimum_required(VERSION 3.10)
project(MallocAllocator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2")

add_executable(code malloc.c)
