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

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
else()# if (CMAKE_BUILD_TYPE STREQUAL "ReleaseWithDebugInfo")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3")
endif ()

set(CMAKE_CXX_STANDARD 20)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/contrib)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/contrib/numactl)

add_subdirectory(contrib)
add_subdirectory(exec)
add_subdirectory(benchmarks)