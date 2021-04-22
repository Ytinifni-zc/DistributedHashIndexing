cmake_minimum_required(VERSION 3.0)
project(DistributedHashIndexing)

include(cmake/arch.cmake)
include(cmake/tools.cmake)

if (CMAKE_GENERATOR STREQUAL "Ninja")
    # Turn on colored output. https://github.com/ninja-build/ninja/wiki/FAQ
    if (COMPILER_GCC)
        add_compile_options(-fdiagnostics-color=always)
    endif ()
    if (COMPILER_CLANG)
        add_compile_options(-fcolor-diagnostics)
    endif ()
endif ()

set(CMAKE_CXX_STANDARD 20)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/contrib)

add_subdirectory(contrib)
add_subdirectory(exec)
add_subdirectory(benchmarks)