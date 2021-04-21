# This dir is copy from ${CMAKE_SOURCE_DIR}/contrib/fastfilter_cpp/benchmarks


if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif ()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

add_executable(BFBenchmark bulk-insert-and-query.cc linux-perf-events.h random.h timing.h)

# BloomFilters
LIST(APPEND BLOOM_FILTERS_INCLUDE_PATH
        ${CMAKE_SOURCE_DIR}/contrib/fastfilter_cpp/src
        ${CMAKE_SOURCE_DIR}/contrib/fastfilter_cpp/src/bloom
        ${CMAKE_SOURCE_DIR}/contrib/fastfilter_cpp/src/cuckoo
        ${CMAKE_SOURCE_DIR}/contrib/fastfilter_cpp/src/gcs
        ${CMAKE_SOURCE_DIR}/contrib/fastfilter_cpp/src/gqf
        ${CMAKE_SOURCE_DIR}/contrib/fastfilter_cpp/src/morton
        ${CMAKE_SOURCE_DIR}/contrib/fastfilter_cpp/src/xorfilter
        )
#message(STATUS ${BLOOM_FILTERS_INCLUDE_PATH})
target_include_directories(BFBenchmark PRIVATE ${BLOOM_FILTERS_INCLUDE_PATH})
