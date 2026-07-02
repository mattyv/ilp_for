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

# Test both modes
run_tests "ILP (default)" ""

# Compile-fail harness (ILP_END/ILP_END_RETURN enforcement) is meaningful only
# in default mode - under ILP_MODE_SIMPLE the mismatch legitimately compiles.
echo "=========================================="
echo "Testing: compile-fail harness (default mode only)"
echo "=========================================="
bash compile_fail/check_compile_fail.sh
echo ""

run_tests "SIMPLE" "-DCMAKE_CXX_FLAGS=-DILP_MODE_SIMPLE"

echo "=========================================="
echo "All modes passed!"
echo "=========================================="
