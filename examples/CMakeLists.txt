cmake_minimum_required(VERSION 3.14)
project(ilp_for_examples)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

# Include the main library
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Example executables
add_executable(find_first_match find_first_match.cpp)
add_executable(parallel_min_max parallel_min_max.cpp)
add_executable(validate_data validate_data.cpp)
add_executable(count_occurrences count_occurrences.cpp)

# Optimization flags
foreach(target find_first_match parallel_min_max validate_data count_occurrences)
    target_compile_options(${target} PRIVATE -O3 -march=native)
endforeach()
