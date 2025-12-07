#!/bin/bash
# Run benchmarks with different compilers and ILP modes
# Usage: ./bench_all.sh [--quick]
#   --quick: Only run ILP mode (skip SIMPLE and PRAGMA)

set -e

QUICK_MODE=false
if [[ "$1" == "--quick" || "$1" == "-q" ]]; then
    QUICK_MODE=true
    echo "Quick mode: only running ILP benchmarks"
fi

cd "$(dirname "$0")"

# On Windows (MSYS2/Git Bash), ensure MSYS2 runtime is found first to avoid DLL conflicts
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    if [[ -d "/c/msys64/ucrt64/bin" ]]; then
        export PATH="/c/msys64/ucrt64/bin:$PATH"
    elif [[ -d "/c/msys64/mingw64/bin" ]]; then
        export PATH="/c/msys64/mingw64/bin:$PATH"
    fi
fi

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

    cmake --build . --config Release -j

    # Run benchmark and save results
    local result_file="../${RESULTS_DIR}/${name}_${TIMESTAMP}.json"
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
        ./bench_reduce.exe --benchmark_out="$result_file" --benchmark_out_format=json
    else
        ./bench_reduce --benchmark_out="$result_file" --benchmark_out_format=json
    fi

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
    if [ "$QUICK_MODE" = false ]; then
        run_benchmark "clang_simple" "$CLANG_CMD" "-O3 -march=native -DILP_MODE_SIMPLE"
    fi
fi

if [ -n "$GCC_CMD" ]; then
    run_benchmark "gcc_ilp" "$GCC_CMD" "-O3 -march=native"
    if [ "$QUICK_MODE" = false ]; then
        run_benchmark "gcc_simple" "$GCC_CMD" "-O3 -march=native -DILP_MODE_SIMPLE"
    fi
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
