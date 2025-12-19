cmake_minimum_required(VERSION 3.14)
project(ilp_for_examples)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

# Include the main library
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Example executables
add_executable(process_with_break process_with_break.cpp)
add_executable(find_and_return find_and_return.cpp)

# All example targets
set(EXAMPLE_TARGETS
    process_with_break
    find_and_return
)

# Optimization flags (platform-specific)
foreach(target ${EXAMPLE_TARGETS})
    if(MSVC)
        target_compile_options(${target} PRIVATE /O2 /W4)
    else()
        target_compile_options(${target} PRIVATE -O3 -march=native)
    endif()
endforeach()
