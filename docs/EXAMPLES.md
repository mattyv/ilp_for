# Assembly Examples

Don't take the README's word for it — these Compiler Explorer (Godbolt) links show the generated assembly side by side.

Each example compiles three versions of the same loop:
- **ILP**: multi-accumulator pattern with parallel operations
- **Hand-rolled**: manual 4x unroll, for comparison
- **Simple**: baseline one-element-at-a-time loop

---

## Loop with Break

ILP_FOR with ILP_BREAK showing early exit from unrolled loop

**View on Godbolt:** [x86-64 Clang (MCA)](https://godbolt.org/z/r61eMvfd7) | [x86-64 GCC](https://godbolt.org/z/9rc5GoevP) | [ARM64](https://godbolt.org/z/7WM61Ea96)

[Source code](../godbolt_examples/loop_with_break.cpp)

---

## Pragma Unroll vs ILP_FOR

Why #pragma unroll doesn't help for early-exit loops - look for per-iteration bounds checks

**View on Godbolt:** [x86-64 Clang (MCA)](https://godbolt.org/z/Eenb8P83T) | [x86-64 GCC](https://godbolt.org/z/7nhsh4W8z) | [ARM64](https://godbolt.org/z/PbfceW1P6)

[Source code](../godbolt_examples/pragma_vs_ilp.cpp)

---

## Loop with Return

ILP_FOR with ILP_RETURN to exit enclosing function from loop

**View on Godbolt:** [x86-64 Clang (MCA)](https://godbolt.org/z/dYs45d5T4) | [x86-64 GCC](https://godbolt.org/z/es9ao7W5c) | [ARM64](https://godbolt.org/z/7Pxj6eMKW)

[Source code](../godbolt_examples/loop_with_return.cpp)

---

## Loop with Large Return Type

ILP_FOR_T for return types > 8 bytes (structs, large objects)

**View on Godbolt:** [x86-64 Clang (MCA)](https://godbolt.org/z/K6EPf58v5) | [x86-64 GCC](https://godbolt.org/z/3exGvo9Yr) | [ARM64](https://godbolt.org/z/P3vjbTdEG)

[Source code](../godbolt_examples/loop_with_return_typed.cpp)

---


## How to Use

1. Pick the Godbolt link for your target architecture — the code loads with optimizations already enabled
2. Compare the assembly across the three implementations
3. Worth looking for:
   - Comparisons issued back-to-back in the ILP version vs interleaved bounds checks in the others
   - Where the bounds check lands: once per block (ILP) vs once per element (pragma)
   - Register usage and instruction scheduling differences

## Compiler Settings

- **x86-64 Clang**: Clang 18, `-std=c++2b -O3 -march=skylake`
- **x86-64 GCC**: GCC 14.1, `-std=c++2b -O3 -march=skylake`
- **ARM64**: ARM Clang 18, `-std=c++2b -O3 -mcpu=apple-m1`
