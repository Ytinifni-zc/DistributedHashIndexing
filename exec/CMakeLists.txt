file(GLOB EXEC_FILE *.cpp)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
if (COMPILER_GCC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
elseif(COMPILER_CLANG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp-optimistic-collapse")
endif()

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
#message(STATUS "BLOOM_FILTERS_INCLUDE_PATH is: ${BLOOM_FILTERS_INCLUDE_PATH}")

# RPCLib
set(RPCLIB_INCLUDE_PATH "${ClickHouse_SOURCE_DIR}/contrib/rpclib" CACHE STRING "Path of rpclib library")
#message(STATUS "RPCLIB_INCLUDE_PATH is ${RPCLIB_INCLUDE_PATH}")

foreach (exe ${EXEC_FILE})
    get_filename_component(name ${exe} NAME_WE)
    add_executable(${name} ${exe})# ArgsParser.h)

    target_include_directories(${name} PRIVATE ${BLOOM_FILTERS_INCLUDE_PATH})
    target_include_directories(${name} PRIVATE ${RPCLIB_INCLUDE_PATH})
    target_link_libraries(${name} rpc pthread dl numa)
endforeach ()