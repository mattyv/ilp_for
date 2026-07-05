# Assembly Examples

Don't take the README's word for it — these Compiler Explorer (Godbolt) links show the generated assembly side by side.

Each example compiles three versions of the same loop:
- **ILP**: multi-accumulator pattern with parallel operations
- **Hand-rolled**: manual 4x unroll, for comparison
- **Simple**: baseline one-element-at-a-time loop

---

## Loop with Break

ILP_FOR with ILP_BREAK showing early exit from unrolled loop

**View on Godbolt:** [x86-64 Clang (MCA)](https://godbolt.org/z/815Efz33d) | [x86-64 GCC](https://godbolt.org/z/1vfj6Td8z) | [ARM64](https://godbolt.org/z/4E1dad9PK)

[Source code](../godbolt_examples/loop_with_break.cpp)

---

## Pragma Unroll vs ILP_FOR

Why #pragma unroll doesn't help for early-exit loops - look for per-iteration bounds checks

**View on Godbolt:** [x86-64 Clang (MCA)](https://godbolt.org/z/jEEjh7jzv) | [x86-64 GCC](https://godbolt.org/z/afWKWeqa5) | [ARM64](https://godbolt.org/z/xdodz4xbr)

[Source code](../godbolt_examples/pragma_vs_ilp.cpp)

---

## Loop with Return

ILP_FOR with ILP_RETURN to exit enclosing function from loop

**View on Godbolt:** [x86-64 Clang (MCA)](https://godbolt.org/z/Wba3Ec4M7) | [x86-64 GCC](https://godbolt.org/z/6eaP9eTqx) | [ARM64](https://godbolt.org/z/3bG33TvPa)

[Source code](../godbolt_examples/loop_with_return.cpp)

---

## Loop with Large Return Type

ILP_FOR_T for return types > 8 bytes (structs, large objects)

**View on Godbolt:** [x86-64 Clang (MCA)](https://godbolt.org/z/fPbYr5so6) | [x86-64 GCC](https://godbolt.org/z/MG3E7nq6q) | [ARM64](https://godbolt.org/z/ef751df7n)

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

- **x86-64 Clang (MCA)**: Clang 18.1, `-std=c++20 -O3 -march=skylake`, with an llvm-mca pane (`-mcpu=skylake`)
- **x86-64 GCC**: GCC 14.1, `-std=c++20 -O3 -march=skylake`
- **ARM64**: armv8-a Clang 18.1, `-std=c++20 -O3 -mcpu=apple-m1`

The links above are generated from the current `godbolt_examples/*.cpp` sources
by `godbolt_examples/make_godbolt_links.py`; rerun it after editing an example
to refresh them.
