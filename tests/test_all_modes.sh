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
    make -j

    ./test_runner

    cd ..
    echo ""
}

# Test all three modes
run_tests "ILP (default)" ""
run_tests "SIMPLE" "-DCMAKE_CXX_FLAGS=-DILP_MODE_SIMPLE"
run_tests "PRAGMA" "-DCMAKE_CXX_FLAGS=-DILP_MODE_PRAGMA"

echo "=========================================="
echo "All modes passed!"
echo "=========================================="
