#!/bin/bash
# Compile-fail harness for ILP_END/ILP_END_RETURN enforcement (and, via the
# optional EXTRA_FLAGS line, other compile-time-observable properties like
# -Wshadow behavior).
#
# Each .cpp file's first line declares its expectation; optional following
# lines (2nd/3rd, order doesn't matter between them) add compile flags or
# restrict which compiler the case applies to:
#   // COMPILE_FAIL: <substring expected in the compiler's stderr>
#   // COMPILE_OK
#   // EXTRA_FLAGS: <flags appended to the compile command>
#   // GCC_ONLY   (skip this file entirely when $CXX is Clang - for cases that
#                  probe a GCC-specific diagnostic, e.g. a [[deprecated]]
#                  warning gated on __GNUC__ && !__clang__, which simply never
#                  fires under Clang and would otherwise make an expected
#                  COMPILE_FAIL wrongly succeed there)
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

# Detect once whether $CXX is Clang, for GCC_ONLY skipping below.
IS_CLANG=0
if "$CXX" -dM -E -x c++ /dev/null 2>/dev/null | grep -q '__clang__'; then
    IS_CLANG=1
fi

for src in *.cpp; do
    # Strip trailing CR so CRLF-saved test files don't produce baffling
    # failures (an invisible \r makes "// COMPILE_OK" unrecognizable and
    # poisons an EXTRA_FLAGS/GCC_ONLY line).
    expectation=$(sed -n '1p' "$src"); expectation="${expectation%$'\r'}"

    # EXTRA_FLAGS and GCC_ONLY may each appear on line 2 or 3, in either order.
    header=$(sed -n '2,3p' "$src" | sed 's/\r$//')

    extra_flags=""
    extra_line=$(grep -m1 '^// EXTRA_FLAGS:' <<< "$header") || true
    if [[ -n "$extra_line" ]]; then
        extra_flags="${extra_line#// EXTRA_FLAGS:}"
        extra_flags="${extra_flags# }" # tolerate both "FLAGS: -x" and "FLAGS:-x"
    fi

    if grep -q '^// GCC_ONLY$' <<< "$header" && [[ $IS_CLANG -eq 1 ]]; then
        echo "SKIP: $src (GCC-only case, running under Clang)"
        continue
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
