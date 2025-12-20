cmake_minimum_required(VERSION 3.16)
project(ilp_clang_tidy_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Test executable
add_executable(clang_tidy_test_runner
    test_clang_tidy.cpp
)

# Include Catch2 header from main tests directory
target_include_directories(clang_tidy_test_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../tests/correctness
)

# Test target
add_custom_target(test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/clang_tidy_test_runner
    DEPENDS clang_tidy_test_runner
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    COMMENT "Running clang-tidy tests"
)
