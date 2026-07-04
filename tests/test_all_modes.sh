#!/bin/bash
# Test all ILP modes

set -e

BUILD_DIR="build"
cd "$(dirname "$0")"

run_tests() {
    local mode_name="$1"
    local cmake_flags="$2"

    echo "=========================================="
    echo "Testing: $mode_name"
    echo "=========================================="

    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    cmake .. $cmake_flags
    cmake --build . -j

    ./test_runner

    cd ..
    echo ""
}

# The macro layer is unconditional (ILP_MODE_SIMPLE only flips ilp::default_mode
# - see mode.hpp), so the compile-fail and runtime-fail harnesses are meaningful
# in both modes and run once per mode below.

run_tests "ILP (default)" ""

echo "=========================================="
echo "Testing: compile-fail harness (default mode)"
echo "=========================================="
bash compile_fail/check_compile_fail.sh
echo ""

echo "=========================================="
echo "Testing: runtime-fail harness (default mode)"
echo "=========================================="
bash runtime_fail/check_runtime_fail.sh
echo ""

run_tests "SIMPLE" "-DCMAKE_CXX_FLAGS=-DILP_MODE_SIMPLE"

echo "=========================================="
echo "Testing: compile-fail harness (SIMPLE mode)"
echo "=========================================="
ILP_EXTRA_CXXFLAGS="-DILP_MODE_SIMPLE" bash compile_fail/check_compile_fail.sh
echo ""

echo "=========================================="
echo "Testing: runtime-fail harness (SIMPLE mode)"
echo "=========================================="
ILP_EXTRA_CXXFLAGS="-DILP_MODE_SIMPLE" bash runtime_fail/check_runtime_fail.sh
echo ""

echo "=========================================="
echo "All modes passed!"
echo "=========================================="
