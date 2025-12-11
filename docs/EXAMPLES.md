# Assembly Examples

View side-by-side comparisons on Compiler Explorer (Godbolt).

Each example shows three versions:
- **ILP**: Multi-accumulator pattern with parallel operations
- **Hand-rolled**: Manual 4x unroll for comparison
- **Simple**: Baseline single-iteration loop

---

# ILP_FOR Macro

The primary API for loops with `break`, `continue`, or `return`.

## Loop with Break

ILP_FOR with ILP_BREAK showing early exit from unrolled loop

**View on Godbolt:** [x86-64 Clang (MCA)](https://godbolt.org/z/r61eMvfd7) | [x86-64 GCC](https://godbolt.org/z/9rc5GoevP) | [ARM64](https://godbolt.org/z/7WM61Ea96)

[Source code](../godbolt_examples/loop_with_break.cpp)

---

## Pragma Unroll vs ILP_FOR

Why #pragma unroll doesn't help for early-exit loops - look for per-iteration bounds checks

**View on Godbolt:** [x86-64 Clang (MCA)](https://godbolt.org/z/Mh4aTP5j7) | [x86-64 GCC](https://godbolt.org/z/6najPjd3a) | [ARM64](https://godbolt.org/z/7dnxEoWbc)

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

# Function API

Alternative `std::`-style functions with early exit support.

## Find First Match

ilp::find for early-exit search (std::find alternative)

**View on Godbolt:** [x86-64 Clang (MCA)](https://godbolt.org/z/rM16zfvW1) | [x86-64 GCC](https://godbolt.org/z/GbhM5njcv) | [ARM64](https://godbolt.org/z/hfPhvKjKd)

[Source code](../godbolt_examples/find_first_match.cpp)

---

## Parallel Minimum

ilp::reduce breaking dependency chains (std::min_element alternative)

**View on Godbolt:** [x86-64 Clang (MCA)](https://godbolt.org/z/ffWb68TYs) | [x86-64 GCC](https://godbolt.org/z/vsjMddvWo) | [ARM64](https://godbolt.org/z/7c9oe1bjM)

[Source code](../godbolt_examples/parallel_min.cpp)

---

## Sum with Early Exit

ilp::reduce with early termination (std::accumulate alternative)

**View on Godbolt:** [x86-64 Clang (MCA)](https://godbolt.org/z/bMhKqf1Kh) | [x86-64 GCC](https://godbolt.org/z/cqMerK4hG) | [ARM64](https://godbolt.org/z/K716KaYd5)

[Source code](../godbolt_examples/sum_with_break.cpp)

---


## How to Use

1. Click a Godbolt link for your target architecture
2. The code will load with optimizations enabled
3. Compare the generated assembly between implementations
4. Look for:
   - Parallel comparisons in ILP version vs sequential in hand-rolled
   - Register usage and instruction scheduling differences
   - Branch prediction patterns

## Compiler Settings

- **x86-64 Clang**: Clang 18, `-std=c++2b -O3 -march=skylake`
- **x86-64 GCC**: GCC 14.1, `-std=c++2b -O3 -march=skylake`
- **ARM64**: ARM Clang 18, `-std=c++2b -O3 -mcpu=apple-m1`
