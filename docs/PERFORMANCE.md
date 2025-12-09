# Performance Notes

This library's goal is not to be a performance library, but to ensure performance doesn't stop you from using it. The patterns here generate efficient code comparable to hand-written unrolled loops.

## ILP_FOR Benchmarks

Apple M2, Clang 19, 10M elements, `-O3 -march=native`

| Loop Type | Simple | ILP | Speedup |
|-----------|--------|-----|---------|
| `ILP_FOR` with `ILP_BREAK` | 1.49ms | 1.15ms | **1.29x** |
| `ILP_FOR` with `ILP_RETURN` | 1.66ms | 0.94ms | **1.77x** |

### Why ILP_RETURN is Faster

`ILP_RETURN` allows the compiler to hoist comparisons before the conditional return logic:

```cpp
// ILP pattern - comparisons run in parallel
ILP_FOR(auto i, 0, n, 4) {
    if (data[i] == target) ILP_RETURN(i);
} ILP_END_RETURN;

// Conceptually:
bool b0 = data[i+0] == target;  // parallel
bool b1 = data[i+1] == target;  // parallel
bool b2 = data[i+2] == target;  // parallel
bool b3 = data[i+3] == target;  // parallel
// then sequential check and return
```

## Function API Benchmarks

For `ilp::find` and `ilp::reduce` (alternative API):

| Operation | std | ILP | Speedup |
|-----------|-----|-----|---------|
| Min (`ilp::reduce`) | 3.47ms | 0.60ms | **5.8x** |
| Sum with break | 1.81ms | 1.11ms | **1.6x** |
| Any-of | 1.99ms | 0.93ms | **2.1x** |
| Find | 0.93ms | 0.93ms | ~1.0x |

The 5.8x speedup on min comes from breaking the dependency chain - `std::min_element` must compare sequentially, while `ilp::reduce` maintains N independent accumulators.

## When ILP Helps

**Good candidates:**
- Loops with early exit (`ILP_BREAK`, `ILP_RETURN`)
- Operations with dependency chains (min, max)
- Search operations where comparisons can run in parallel

**Skip ILP for:**
- Simple sums without early exit - compilers auto-vectorize better
- Operations where the compiler already optimizes well

```cpp
// Use ILP - early exit benefits from parallel evaluation
ILP_FOR(auto i, 0, n, 4) {
    if (expensive_check(data[i])) ILP_RETURN(result);
} ILP_END_RETURN;

// Skip ILP - compiler auto-vectorizes better
int sum = std::accumulate(data.begin(), data.end(), 0);
```

## Why Not `#pragma unroll`?

Pragma unroll duplicates loop bodies but doesn't parallelize conditional checks:

```cpp
// #pragma unroll generates sequential dependency chain:
for (int i = 0; i < n; i++) {
    if (data[i] == target) return i;  // check 0
    if (data[i+1] == target) return i+1;  // waits for check 0
    if (data[i+2] == target) return i+2;  // waits for check 1
    if (data[i+3] == target) return i+3;  // waits for check 2
}

// ILP multi-accumulator runs checks in parallel:
bool found0 = data[i] == target;    // parallel
bool found1 = data[i+1] == target;  // parallel
bool found2 = data[i+2] == target;  // parallel
bool found3 = data[i+3] == target;  // parallel
if (found0 | found1 | found2 | found3) { /* find which */ }
```
