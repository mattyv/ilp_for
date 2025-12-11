cmake_minimum_required(VERSION 3.16)
project(ilp_for_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

# Platform-specific compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /W4 /EHsc")
    add_compile_options(/Zc:__cplusplus /permissive-)
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -Wextra")
endif()

# Coverage option
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

if(ENABLE_COVERAGE)
    message(STATUS "Code coverage enabled")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# Sanitizer options (GCC/Clang only)
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(ENABLE_UBSAN "Enable Undefined Behavior Sanitizer" OFF)

if(NOT MSVC)
    if(ENABLE_ASAN)
        message(STATUS "Address Sanitizer enabled (aggressive)")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -fsanitize-address-use-after-scope")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        # Recommended ASAN_OPTIONS for CI: detect_leaks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1
    endif()

    if(ENABLE_UBSAN)
        message(STATUS "Undefined Behavior Sanitizer enabled (aggressive)")
        # Include additional checks beyond basic 'undefined':
        # - float-divide-by-zero: catch FP division by zero
        # - unsigned-integer-overflow: catch unsigned wraparound (often unintended)
        # - local-bounds: array bounds checking for stack arrays
        # - nullability: null pointer dereference checks
        # - pointer-overflow: catch pointer arithmetic wraparound (important for loops)
        # Note: implicit-conversion omitted - too noisy with templates/Catch2
        set(UBSAN_FLAGS "-fsanitize=undefined,float-divide-by-zero,unsigned-integer-overflow,local-bounds,nullability,pointer-overflow")
        # Make UBSan errors fatal - halt on first error instead of continuing
        set(UBSAN_FLAGS "${UBSAN_FLAGS} -fno-sanitize-recover=all")
        # Ignorelist for false positives in system libraries (libstdc++ string::compare)
        set(UBSAN_FLAGS "${UBSAN_FLAGS} -fsanitize-ignorelist=${CMAKE_CURRENT_SOURCE_DIR}/ubsan_suppressions.txt")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${UBSAN_FLAGS} -fno-omit-frame-pointer")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
    endif()
endif()

# Include parent directory for ilp_for.hpp
include_directories(..)

# Collect sources
file(GLOB TEST_SRCS correctness/*.cpp)

# Test runner executable
add_executable(test_runner ${TEST_SRCS})

# Test target
add_custom_target(test
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_runner
    DEPENDS test_runner
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running all tests"
)

# Test all ILP modes (default, SIMPLE, PRAGMA)
add_custom_target(test-all-modes
    COMMAND ${CMAKE_COMMAND} -E echo "Testing all ILP modes..."
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/test_all_modes.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running tests for all ILP modes"
)

# Coverage targets (only available when ENABLE_COVERAGE is ON)
if(ENABLE_COVERAGE)
    # Clean coverage data
    add_custom_target(coverage-clean
        COMMAND find ${CMAKE_CURRENT_BINARY_DIR} -name "*.gcda" -delete
        COMMAND find ${CMAKE_CURRENT_BINARY_DIR} -name "*.gcov" -delete
        COMMAND rm -rf ${CMAKE_CURRENT_BINARY_DIR}/coverage.info
        COMMAND rm -rf ${CMAKE_CURRENT_BINARY_DIR}/coverage_html
        COMMENT "Cleaning coverage data"
    )

    # Run tests with coverage
    add_custom_target(coverage-run
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_runner
        DEPENDS test_runner
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running tests with coverage instrumentation"
    )

    # Generate lcov report
    add_custom_target(coverage-report
        COMMAND lcov --capture --directory ${CMAKE_CURRENT_BINARY_DIR} --output-file ${CMAKE_CURRENT_BINARY_DIR}/coverage.info --ignore-errors inconsistent,unused,mismatch,negative,format --rc branch_coverage=0
        COMMAND lcov --remove ${CMAKE_CURRENT_BINARY_DIR}/coverage.info '/usr/*' '/Library/*' '*/catch.hpp' '*/tests/*' --output-file ${CMAKE_CURRENT_BINARY_DIR}/coverage.info --ignore-errors unused,inconsistent,empty
        COMMAND lcov --list ${CMAKE_CURRENT_BINARY_DIR}/coverage.info --ignore-errors inconsistent
        DEPENDS coverage-run
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating lcov coverage report"
    )

    # Generate HTML coverage report
    add_custom_target(coverage-html
        COMMAND genhtml ${CMAKE_CURRENT_BINARY_DIR}/coverage.info --output-directory ${CMAKE_CURRENT_BINARY_DIR}/coverage_html
        COMMAND ${CMAKE_COMMAND} -E echo "Coverage report generated at: ${CMAKE_CURRENT_BINARY_DIR}/coverage_html/index.html"
        DEPENDS coverage-report
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating HTML coverage report"
    )

    # All-in-one coverage target
    add_custom_target(coverage
        DEPENDS coverage-html
        COMMENT "Generate complete coverage report"
    )
endif()
