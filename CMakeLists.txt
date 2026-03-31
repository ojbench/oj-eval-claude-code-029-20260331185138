cmake_minimum_required(VERSION 3.10)
project(MallocAllocator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -fPIC")

# Build shared library that can be LD_PRELOADed
add_library(code_lib SHARED malloc.c)
set_target_properties(code_lib PROPERTIES PREFIX "" OUTPUT_NAME "code" SUFFIX "")
