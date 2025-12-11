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
        message(STATUS "Address Sanitizer enabled")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    endif()

    if(ENABLE_UBSAN)
        message(STATUS "Undefined Behavior Sanitizer enabled")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fno-omit-frame-pointer")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
    endif()
endif()

# Include parent directory for ilp_for.hpp
include_directories(..)

# Collect sources
file(GLOB TEST_SRCS correctness/*.cpp)

# Test runner executable
if(MSVC)
    # MSVC: Only build correctness tests (asm_compare uses GCC attributes)
    add_executable(test_runner ${TEST_SRCS})
else()
    # Unix: Include asm_compare object libraries
    file(GLOB HANDROLLED_SRCS asm_compare/handrolled/*.cpp)
    file(GLOB ILP_SRCS asm_compare/ilp/*.cpp)
    file(GLOB SIMPLE_SRCS asm_compare/simple/*.cpp)
    file(GLOB STD_SRCS asm_compare/std/*.cpp)

    add_library(handrolled OBJECT ${HANDROLLED_SRCS})
    add_library(ilp OBJECT ${ILP_SRCS})
    add_library(simple OBJECT ${SIMPLE_SRCS})
    add_library(std_impl OBJECT ${STD_SRCS})

    add_executable(test_runner
        ${TEST_SRCS}
        $<TARGET_OBJECTS:handrolled>
        $<TARGET_OBJECTS:ilp>
        $<TARGET_OBJECTS:simple>
        $<TARGET_OBJECTS:std_impl>
    )
endif()

# Custom target to generate assembly files (Unix only - uses GCC/Clang flags)
if(NOT MSVC)
    set(ASM_TARGETS)

    foreach(src ${HANDROLLED_SRCS})
        get_filename_component(name ${src} NAME_WE)
        set(asm_file ${CMAKE_CURRENT_SOURCE_DIR}/asm_compare/handrolled/${name}.s)
        add_custom_command(
            OUTPUT ${asm_file}
            COMMAND ${CMAKE_CXX_COMPILER} -O2 -march=native -std=c++20 -I${CMAKE_CURRENT_SOURCE_DIR}/.. -S ${src} -o ${asm_file}
            DEPENDS ${src}
            COMMENT "Generating assembly for handrolled/${name}"
        )
        list(APPEND ASM_TARGETS ${asm_file})
    endforeach()

    foreach(src ${ILP_SRCS})
        get_filename_component(name ${src} NAME_WE)
        set(asm_file ${CMAKE_CURRENT_SOURCE_DIR}/asm_compare/ilp/${name}.s)
        add_custom_command(
            OUTPUT ${asm_file}
            COMMAND ${CMAKE_CXX_COMPILER} -O2 -march=native -std=c++20 -I${CMAKE_CURRENT_SOURCE_DIR}/.. -S ${src} -o ${asm_file}
            DEPENDS ${src}
            COMMENT "Generating assembly for ilp/${name}"
        )
        list(APPEND ASM_TARGETS ${asm_file})
    endforeach()

    foreach(src ${SIMPLE_SRCS})
        get_filename_component(name ${src} NAME_WE)
        set(asm_file ${CMAKE_CURRENT_SOURCE_DIR}/asm_compare/simple/${name}.s)
        add_custom_command(
            OUTPUT ${asm_file}
            COMMAND ${CMAKE_CXX_COMPILER} -O2 -march=native -std=c++20 -I${CMAKE_CURRENT_SOURCE_DIR}/.. -S ${src} -o ${asm_file}
            DEPENDS ${src}
            COMMENT "Generating assembly for simple/${name}"
        )
        list(APPEND ASM_TARGETS ${asm_file})
    endforeach()

    foreach(src ${STD_SRCS})
        get_filename_component(name ${src} NAME_WE)
        set(asm_file ${CMAKE_CURRENT_SOURCE_DIR}/asm_compare/std/${name}.s)
        add_custom_command(
            OUTPUT ${asm_file}
            COMMAND ${CMAKE_CXX_COMPILER} -O2 -march=native -std=c++20 -I${CMAKE_CURRENT_SOURCE_DIR}/.. -S ${src} -o ${asm_file}
            DEPENDS ${src}
            COMMENT "Generating assembly for std/${name}"
        )
        list(APPEND ASM_TARGETS ${asm_file})
    endforeach()

    add_custom_target(asm DEPENDS ${ASM_TARGETS})
endif()

# Test targets
if(MSVC)
    add_custom_target(test
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_runner
        DEPENDS test_runner
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running all tests"
    )
else()
    add_custom_target(test
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_runner
        DEPENDS test_runner asm
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running all tests"
    )

    add_custom_target(test-asm
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/test_runner "[asm]"
        DEPENDS test_runner asm
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running assembly size tests"
    )
endif()

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
        COMMAND lcov --remove ${CMAKE_CURRENT_BINARY_DIR}/coverage.info '/usr/*' '/Library/*' '*/catch.hpp' '*/tests/*' '*/asm_compare/*' --output-file ${CMAKE_CURRENT_BINARY_DIR}/coverage.info --ignore-errors unused,inconsistent,empty
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
