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
add_executable(search_until search_until.cpp)
add_executable(process_with_break process_with_break.cpp)
add_executable(find_and_return find_and_return.cpp)
add_executable(range_with_index range_with_index.cpp)
add_executable(reduce_with_early_exit reduce_with_early_exit.cpp)
add_executable(manual_unroll_factor manual_unroll_factor.cpp)

# All example targets
set(EXAMPLE_TARGETS
    find_first_match
    parallel_min_max
    validate_data
    count_occurrences
    search_until
    process_with_break
    find_and_return
    range_with_index
    reduce_with_early_exit
    manual_unroll_factor
)

# Optimization flags (platform-specific)
foreach(target ${EXAMPLE_TARGETS})
    if(MSVC)
        target_compile_options(${target} PRIVATE /O2 /W4)
    else()
        target_compile_options(${target} PRIVATE -O3 -march=native)
    endif()
endforeach()
