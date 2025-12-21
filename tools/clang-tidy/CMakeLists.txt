# ilp_for - ILP loop unrolling for C++20
# Copyright (c) 2025 Matt Vanderdorff
# https://github.com/mattyv/ilp_for
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.14)
project(ilp-clang-tidy)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find LLVM and Clang
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# Include paths
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../)

add_definitions(${LLVM_DEFINITIONS})

# The clang-tidy plugin
add_library(ILPTidyModule MODULE
    ILPLoopCheck.cpp
    ILPTidyModule.cpp
)

# Suppress duplicate library warnings on macOS
if(APPLE)
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,-no_warn_duplicate_libraries")
endif()

# Link only top-level libraries - they bring in dependencies
target_link_libraries(ILPTidyModule PRIVATE
    clangTidy
    clangTidyUtils
    clangASTMatchers
)

# Set output name
set_target_properties(ILPTidyModule PROPERTIES
    OUTPUT_NAME "ILPTidyModule"
    PREFIX ""
)

# Note: To use with clang-tidy, load the module with:
#   clang-tidy -load ./ILPTidyModule.so -checks='ilp-*' file.cpp
