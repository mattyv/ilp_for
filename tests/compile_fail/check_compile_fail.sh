#!/bin/bash
# Compile-fail harness for ILP_END/ILP_END_RETURN enforcement (and, via the
# optional EXTRA_FLAGS line, other compile-time-observable properties like
# -Wshadow behavior).
#
# Each .cpp file's first line declares its expectation, with an optional second
# line adding compile flags:
#   // COMPILE_FAIL: <substring expected in the compiler's stderr>
#   // COMPILE_OK
#   // EXTRA_FLAGS: <flags appended to the compile command>   (optional 2nd line)
#
# The macro layer is unconditional (ILP_MODE_SIMPLE only flips ilp::default_mode
# - see mode.hpp), so every case here holds in both build modes. Set
# ILP_EXTRA_CXXFLAGS (e.g. to -DILP_MODE_SIMPLE) to run this harness against a
# non-default mode; test_all_modes.sh does this for its SIMPLE leg.

set -u
cd "$(dirname "$0")"

# Default to the platform's generic C++ driver (present on Linux and macOS alike);
# the CMake compile-fail-tests target overrides this with the configured compiler.
CXX="${CXX:-c++}"
BASE_CXXFLAGS="-std=c++20 -I../.. ${ILP_EXTRA_CXXFLAGS:-}"
FAILED=0

for src in *.cpp; do
    expectation=$(sed -n '1p' "$src")
    second_line=$(sed -n '2p' "$src")

    extra_flags=""
    if [[ "$second_line" == "// EXTRA_FLAGS:"* ]]; then
        extra_flags="${second_line#// EXTRA_FLAGS: }"
    fi

    obj="$(mktemp /tmp/ilp_compile_fail.XXXXXX.o)"
    out=$("$CXX" $BASE_CXXFLAGS $extra_flags -c "$src" -o "$obj" 2>&1)
    status=$?
    rm -f "$obj"

    if [[ "$expectation" == "// COMPILE_OK" ]]; then
        if [[ $status -ne 0 ]]; then
            echo "FAIL: $src expected to compile, but failed:"
            echo "$out"
            FAILED=1
        else
            echo "PASS: $src compiled as expected"
        fi
    elif [[ "$expectation" == "// COMPILE_FAIL:"* ]]; then
        needle="${expectation#// COMPILE_FAIL: }"
        if [[ $status -eq 0 ]]; then
            echo "FAIL: $src expected to fail to compile, but succeeded"
            FAILED=1
        elif [[ "$out" != *"$needle"* ]]; then
            echo "FAIL: $src failed to compile as expected, but stderr did not contain: $needle"
            echo "--- actual output ---"
            echo "$out"
            FAILED=1
        else
            echo "PASS: $src failed to compile with the expected message"
        fi
    else
        echo "FAIL: $src has no recognized expectation header (first line: $expectation)"
        FAILED=1
    fi
done

if [[ $FAILED -ne 0 ]]; then
    echo "=========================================="
    echo "Compile-fail harness: FAILURES DETECTED"
    echo "=========================================="
    exit 1
fi

echo "=========================================="
echo "Compile-fail harness: all cases behaved as expected"
echo "=========================================="
