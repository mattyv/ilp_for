# Assembly Examples

View side-by-side comparisons of ILP_FOR implementations on Compiler Explorer (Godbolt).

Each example shows three versions:
- **ILP_FOR**: Multi-accumulator pattern with parallel operations
- **Hand-rolled**: Manual 4x unroll with sequential dependencies
- **Simple**: Baseline single-iteration loop

Compare assembly output across architectures to see the optimization differences.

---

## Find First Match

Early-exit search comparing ILP multi-accumulator vs sequential checks

**View on Godbolt:** [x86-64 Clang](https://godbolt.org/z/9GeTbxPhh) | [x86-64 GCC](https://godbolt.org/z/GbhM5njcv) | [ARM64](https://godbolt.org/z/hfPhvKjKd)

[Source code](../godbolt_examples/find_first_match.cpp)

---

## Parallel Minimum

Parallel accumulator reduce breaking dependency chains

**View on Godbolt:** [x86-64 Clang](https://godbolt.org/z/s3cY9qaEo) | [x86-64 GCC](https://godbolt.org/z/vsjMddvWo) | [ARM64](https://godbolt.org/z/7c9oe1bjM)

[Source code](../godbolt_examples/parallel_min.cpp)

---

## Sum with Early Exit

Reduce with break condition showing control flow handling

**View on Godbolt:** [x86-64 Clang](https://godbolt.org/z/Webz8EfPf) | [x86-64 GCC](https://godbolt.org/z/cqMerK4hG) | [ARM64](https://godbolt.org/z/K716KaYd5)

[Source code](../godbolt_examples/sum_with_break.cpp)

---

## Simple Transform

In-place transformation without control flow (SIMPLE variant)

**View on Godbolt:** [x86-64 Clang](https://godbolt.org/z/o7qqe4dz8) | [x86-64 GCC](https://godbolt.org/z/TMsf9GM1W) | [ARM64](https://godbolt.org/z/oz64r4411)

[Source code](../godbolt_examples/transform_simple.cpp)

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
