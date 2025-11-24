# Performance Notes

This library's goal is not to be a performance library, but to ensure performance doesn't stop you from using it. The patterns here generate efficient code comparable to hand-written unrolled loops.

## Benchmarks

Clang 17 on Apple M2

| Operation | std | ILP | Speedup |
|-----------|-----|-----|---------|
| Min | 3.4ms | 0.58ms | **5.9x** |
| Any-of | 4.0ms | 0.75ms | **5.3x** |
| Find | 1.9ms | 1.6ms | **1.2x** |
| Sum with break | 1.8ms | 1.1ms | **1.6x** |

### Simple Reductions

| Operation | std/simple | ILP | Result |
|-----------|------------|-----|--------|
| Sum | 0.54ms | 0.58ms | **ILP ~7% slower** |

Modern compilers auto-vectorize simple sums effectively. Use `ILP_REDUCE_SUM` only when you need break/return support.

## Early Return Performance

The `*_RET_SIMPLE` functions auto-detect the optimal mode based on return type.

### Bool Mode (Fastest)

Return `bool` to get the index of the first match. This avoids `csel` (conditional select) dependencies:

```cpp
// Matches std::find performance - use ILP_FOR_UNTIL for find operations
auto idx = ILP_FOR_UNTIL_RANGE_AUTO(val, data) {
    return val == target;  // returns bool
} ILP_END_UNTIL;
// Returns: std::optional<size_t> - index if found, nullopt if not
```

### Optional Mode (General Purpose)

Return `std::optional<T>` for computed values:

```cpp
auto result = ILP_FOR_RET_SIMPLE(i, 0uz, data.size(), 4) {
    if (expensive_check(data[i])) {
        return std::optional(compute(data[i]));
    }
    return std::nullopt;
} ILP_END;
// Returns: std::optional<T>
```

### Why Bool Mode is Faster

When your lambda does `if (cond) return value; return sentinel;`, the compiler generates `csel` instructions:

```cpp
// Slower - generates csel dependency chain
return ILP_FOR_RET_SIMPLE(i, 0uz, n, 4) {
    if (data[i] == target) return i;
    return _ilp_end_;
} ILP_END;
```

Each iteration must conditionally select between two values, creating dependencies that prevent parallel execution.

With bool mode, comparisons run in parallel without dependencies:

```cpp
// Fast - parallel comparisons, no csel
return ILP_FOR_RET_SIMPLE(i, 0uz, n, 4) {
    return data[i] == target;
} ILP_END;
```

## Why Not Just Use `#pragma unroll`?

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
