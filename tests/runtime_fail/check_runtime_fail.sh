#!/bin/bash
# Runtime-fail harness for the debug-mode SBO type check (DESIGN_NOTES.md item 3).
#
# Unlike tests/compile_fail (which asserts a compile error), these cases compile
# successfully and assert on RUNTIME behavior. Each .cpp file's first line
# declares its expectation, with an optional second line adding compile flags:
#   // RUN_ABORT: <substring expected on stderr>
#   // RUN_OK
#   // EXTRA_FLAGS: <flags appended to the compile command>   (optional 2nd line)
#
# Deliberately does NOT define NDEBUG (except where a case's own EXTRA_FLAGS
# does), so the type check defaults to enabled - this harness IS the type
# check's regression net.

set -u
cd "$(dirname "$0")"

# Default to the platform's generic C++ driver; the CMake runtime-fail-tests
# target overrides this with the configured compiler.
CXX="${CXX:-c++}"
BASE_CXXFLAGS="-std=c++20 -I../.."
FAILED=0

for src in *.cpp; do
    expectation=$(sed -n '1p' "$src")
    second_line=$(sed -n '2p' "$src")

    extra_flags=""
    if [[ "$second_line" == "// EXTRA_FLAGS:"* ]]; then
        extra_flags="${second_line#// EXTRA_FLAGS: }"
    fi

    bin="$(mktemp /tmp/ilp_runtime_fail.XXXXXX)"
    compile_out=$("$CXX" $BASE_CXXFLAGS $extra_flags "$src" -o "$bin" 2>&1)
    compile_status=$?

    if [[ $compile_status -ne 0 ]]; then
        echo "FAIL: $src did not compile:"
        echo "$compile_out"
        FAILED=1
        rm -f "$bin"
        continue
    fi

    run_out=$("$bin" 2>&1)
    run_status=$?
    rm -f "$bin"

    if [[ "$expectation" == "// RUN_OK" ]]; then
        if [[ $run_status -ne 0 ]]; then
            echo "FAIL: $src expected to run and exit 0, but exited $run_status:"
            echo "$run_out"
            FAILED=1
        else
            echo "PASS: $src ran and exited 0 as expected"
        fi
    elif [[ "$expectation" == "// RUN_ABORT:"* ]]; then
        needle="${expectation#// RUN_ABORT: }"
        if [[ $run_status -eq 0 ]]; then
            echo "FAIL: $src expected to abort, but exited 0"
            FAILED=1
        elif [[ "$run_out" != *"$needle"* ]]; then
            echo "FAIL: $src aborted (exit $run_status) as expected, but output did not contain: $needle"
            echo "--- actual output ---"
            echo "$run_out"
            FAILED=1
        else
            echo "PASS: $src aborted with the expected message"
        fi
    else
        echo "FAIL: $src has no recognized expectation header (first line: $expectation)"
        FAILED=1
    fi
done

if [[ $FAILED -ne 0 ]]; then
    echo "=========================================="
    echo "Runtime-fail harness: FAILURES DETECTED"
    echo "=========================================="
    exit 1
fi

echo "=========================================="
echo "Runtime-fail harness: all cases behaved as expected"
echo "=========================================="
