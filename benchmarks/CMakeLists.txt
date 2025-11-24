cmake_minimum_required(VERSION 3.14)
project(ilp_for_benchmarks)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

# Fetch Google Benchmark
include(FetchContent)
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

# Include the main library
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Benchmark executable
add_executable(bench_reduce
    bench_reduce.cpp
)

target_link_libraries(bench_reduce
    benchmark::benchmark_main
)

target_compile_options(bench_reduce PRIVATE
    -O3
    -march=native
)
