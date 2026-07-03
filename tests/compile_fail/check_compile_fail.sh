#!/bin/bash
# Compile-fail harness for ILP_END/ILP_END_RETURN enforcement.
#
# Each .cpp file's first line declares its expectation:
#   // COMPILE_FAIL: <substring expected in the compiler's stderr>
#   // COMPILE_OK
#
# Run only against the default ILP mode. In ILP_MODE_SIMPLE, ILP_RETURN
# lowers to a plain `return` inside a plain loop, so the ILP_END/ILP_END_RETURN
# mismatch is semantically harmless there and legitimately compiles - this
# harness would be meaningless under that define.

set -u
cd "$(dirname "$0")"

# Default to the platform's generic C++ driver (present on Linux and macOS alike);
# the CMake compile-fail-tests target overrides this with the configured compiler.
CXX="${CXX:-c++}"
CXXFLAGS="-std=c++20 -I../.."
FAILED=0

for src in *.cpp; do
    expectation=$(head -n1 "$src")
    obj="$(mktemp /tmp/ilp_compile_fail.XXXXXX.o)"
    out=$("$CXX" $CXXFLAGS -c "$src" -o "$obj" 2>&1)
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
