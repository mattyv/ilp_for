#!/bin/bash
# Run benchmarks with different compilers and ILP modes

set -e

cd "$(dirname "$0")"

# Output directory for results
RESULTS_DIR="results"
mkdir -p "$RESULTS_DIR"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)

run_benchmark() {
    local name="$1"
    local compiler="$2"
    local cxx_flags="$3"
    local build_dir="build_${name}"

    echo "=========================================="
    echo "Benchmarking: $name"
    echo "Compiler: $compiler"
    echo "Flags: $cxx_flags"
    echo "=========================================="

    rm -rf "$build_dir"
    mkdir -p "$build_dir"
    cd "$build_dir"

    cmake .. \
        -DCMAKE_CXX_COMPILER="$compiler" \
        -DCMAKE_CXX_FLAGS="$cxx_flags"

    make -j

    # Run benchmark and save results
    local result_file="../${RESULTS_DIR}/${name}_${TIMESTAMP}.json"
    ./bench_reduce --benchmark_out="$result_file" --benchmark_out_format=json

    echo ""
    echo "Results saved to: $result_file"
    echo ""

    cd ..
}

# Detect available compilers
GCC_CMD=""
CLANG_CMD=""

if command -v g++-14 &> /dev/null; then
    GCC_CMD="g++-14"
elif command -v g++-13 &> /dev/null; then
    GCC_CMD="g++-13"
elif command -v g++ &> /dev/null; then
    GCC_CMD="g++"
fi

if command -v clang++ &> /dev/null; then
    CLANG_CMD="clang++"
fi

echo "Found compilers:"
echo "  GCC: ${GCC_CMD:-not found}"
echo "  Clang: ${CLANG_CMD:-not found}"
echo ""

# Run benchmarks
if [ -n "$CLANG_CMD" ]; then
    run_benchmark "clang_ilp" "$CLANG_CMD" "-O3 -march=native"
    run_benchmark "clang_simple" "$CLANG_CMD" "-O3 -march=native -DILP_MODE_SIMPLE"
    run_benchmark "clang_pragma" "$CLANG_CMD" "-O3 -march=native -DILP_MODE_PRAGMA"
fi

if [ -n "$GCC_CMD" ]; then
    run_benchmark "gcc_ilp" "$GCC_CMD" "-O3 -march=native"
    run_benchmark "gcc_simple" "$GCC_CMD" "-O3 -march=native -DILP_MODE_SIMPLE"
    run_benchmark "gcc_pragma" "$GCC_CMD" "-O3 -march=native -DILP_MODE_PRAGMA"
fi

echo "=========================================="
echo "All benchmarks complete!"
echo "Results in: $RESULTS_DIR/"
echo "=========================================="

# Print summary comparison if both compilers ran
if [ -n "$CLANG_CMD" ] && [ -n "$GCC_CMD" ]; then
    echo ""
    echo "To compare results, check the JSON files in $RESULTS_DIR/"
fi
