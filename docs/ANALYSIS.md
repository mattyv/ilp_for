# ILP Library Performance Analysis

## Executive Summary

The ILP library provides **genuine performance benefits** (~12% faster than handrolled equivalents) not through abstraction, but by guiding the compiler to produce better code. The template/fold/lambda pattern acts as an optimization barrier that prevents harmful auto-vectorization.

## Benchmark Results

### Array Sum (10M elements, 100 iterations)

| Implementation | Time (ms) | Notes |
|----------------|-----------|-------|
| **ILP Library** | **130** | Fastest |
| std::accumulate | 145 | Standard library |
| Handrolled (4 accumulators) | 146 | Manual multi-accumulator |
| Simple for loop | 146 | Naive approach |

### With Vectorization Disabled (-fno-tree-vectorize)

| Implementation | With Vectorization | Without | Delta |
|----------------|-------------------|---------|-------|
| Handrolled | 152 ms | 136 ms | -10% faster without! |
| ILP Library | 130 ms | 131 ms | No change |

**Key Finding**: GCC's auto-vectorization hurts the handrolled version by 12%.

## Why the ILP Library is Faster

### The Problem with Auto-Vectorization

When GCC sees explicit multi-accumulator code:
```cpp
sum0 += arr[i];
sum1 += arr[i+1];
sum2 += arr[i+2];
sum3 += arr[i+3];
```

It "optimizes" this into SIMD:
```asm
ldr q30, [x0, x3]           ; load 4 elements into SIMD register
add v31.4s, v31.4s, v30.4s  ; SIMD add
```

This creates a **dependency chain** on the single SIMD accumulator:
- SIMD add latency on Apple M1: ~3 cycles
- Must wait 3 cycles between adds
- Throughput: 4 elements / 3 cycles = **1.33 elements/cycle**

### What the ILP Library Does

The fold expression pattern:
```cpp
((accs[Is] = op(accs[Is], body(ptr[i + Is]))), ...);
```

Produces 4 **independent scalar accumulators**:
```asm
add w5, w5, w9    ; acc0
add w7, w7, w11   ; acc1
add w8, w8, w10   ; acc2
add w6, w6, w9    ; acc3
```

- Scalar add latency: 1 cycle
- All 4 adds are independent (no dependency chain)
- CPU issues all 4 in parallel via out-of-order execution
- Throughput: 4 elements / 1 cycle = **4 elements/cycle**

### Why the Pattern Prevents Vectorization

The combination of:
1. **Lambda call** - obscures data flow
2. **Array indexing with template parameter** - `accs[Is]`
3. **Fold expression expansion** - generates separate statements

Acts as an optimization barrier. GCC cannot see that these 4 array elements could be merged into a SIMD register.

## Why Only 12% Improvement (Not 3x)?

The benchmark is partially **memory-bound**:
- 10M elements × 100 iterations = 1B elements / 0.145s
- = 6.9 billion elements/second
- = ~28 GB/s memory bandwidth
- Apple M1 peak: ~60-70 GB/s

Memory bandwidth limits the speedup from compute improvements.

## Search Operations

For search/find operations with early exit, the library also performs well:

| Implementation | Assembly Lines |
|----------------|---------------|
| **ILP Library** | **167** |
| Simple loop | 220 |
| Handrolled | 233 |
| std::find | 285 |

The unrolled search pattern enables checking multiple elements before branching.

## When to Use the ILP Library

### Recommended Use Cases

1. **Reductions** (sum, product, min/max) - Prevents harmful auto-vectorization
2. **Search operations** - Unrolled comparisons with early exit
3. **When compiler auto-vectorization hurts** - The library acts as a guide rail

### When to Avoid

1. **Simple iterations without reduction** - Use regular loops
2. **Already SIMD-optimized code** - Don't double-optimize
3. **Very small arrays** - Setup overhead not worth it

## Compiler and Architecture Notes

- Tested with GCC 15 on Apple M1 (ARM64)
- Results may vary with different compilers (Clang, MSVC)
- Different CPUs have different SIMD latencies
- Always benchmark for your specific target

## Conclusion

The ILP library provides genuine value beyond syntactic convenience. Its template/fold/lambda pattern guides the compiler to produce optimal code by preventing harmful auto-vectorization. For reduction operations on modern out-of-order CPUs, the multi-accumulator approach with independent dependency chains outperforms SIMD with a single accumulator.

**The library is not overengineered - it's a codegen optimization pattern disguised as an abstraction.**
