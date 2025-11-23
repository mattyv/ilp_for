cmake_minimum_required(VERSION 3.16)
project(ilp_for_tests)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -Wextra")

# Include parent directory for ilp_for.hpp
include_directories(..)

# Collect sources
file(GLOB HANDROLLED_SRCS asm_compare/handrolled/*.cpp)
file(GLOB ILP_SRCS asm_compare/ilp/*.cpp)
file(GLOB SIMPLE_SRCS asm_compare/simple/*.cpp)
file(GLOB STD_SRCS asm_compare/std/*.cpp)
file(GLOB TEST_SRCS correctness/*.cpp)

# Object libraries for asm_compare
add_library(handrolled OBJECT ${HANDROLLED_SRCS})
add_library(ilp OBJECT ${ILP_SRCS})
add_library(simple OBJECT ${SIMPLE_SRCS})
add_library(std_impl OBJECT ${STD_SRCS})

# Test runner executable
add_executable(test_runner
    ${TEST_SRCS}
    $<TARGET_OBJECTS:handrolled>
    $<TARGET_OBJECTS:ilp>
    $<TARGET_OBJECTS:simple>
    $<TARGET_OBJECTS:std_impl>
)

# Custom target to generate assembly files
set(ASM_TARGETS)

foreach(src ${HANDROLLED_SRCS})
    get_filename_component(name ${src} NAME_WE)
    set(asm_file ${CMAKE_CURRENT_SOURCE_DIR}/asm_compare/handrolled/${name}.s)
    add_custom_command(
        OUTPUT ${asm_file}
        COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS} -std=c++23 -I${CMAKE_CURRENT_SOURCE_DIR}/.. -S ${src} -o ${asm_file}
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
        COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS} -std=c++23 -I${CMAKE_CURRENT_SOURCE_DIR}/.. -S ${src} -o ${asm_file}
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
        COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS} -std=c++23 -I${CMAKE_CURRENT_SOURCE_DIR}/.. -S ${src} -o ${asm_file}
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
        COMMAND ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS} -std=c++23 -I${CMAKE_CURRENT_SOURCE_DIR}/.. -S ${src} -o ${asm_file}
        DEPENDS ${src}
        COMMENT "Generating assembly for std/${name}"
    )
    list(APPEND ASM_TARGETS ${asm_file})
endforeach()

add_custom_target(asm DEPENDS ${ASM_TARGETS})

# Test targets
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
