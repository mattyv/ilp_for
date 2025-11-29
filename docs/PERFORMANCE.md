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
// Matches std::find performance - use ILP_FIND_RANGE for find operations
size_t idx = ILP_FIND_RANGE_AUTO(auto&& val, data) {
    return val == target;  // returns bool
} ILP_END;
// Returns: size_t - index if found, size() if not
```

### Optional Mode (General Purpose)

Return early from the enclosing function using `ILP_RETURN`:

```cpp
std::optional<int> find_expensive() {
    ILP_FOR_RET(std::optional<int>, auto i, 0uz, data.size(), 4) {
        if (expensive_check(data[i])) {
            ILP_RETURN(compute(data[i]));
        }
    } ILP_END_RET;
    return std::nullopt;
}
```

### Why Bool Mode is Faster

When using `ILP_FOR_RET` with conditional returns, the compiler generates `csel` instructions:

```cpp
// Slower - generates csel dependency chain
ILP_FOR_RET(size_t, auto i, 0uz, n, 4) {
    if (data[i] == target) ILP_RETURN(i);
} ILP_END_RET;
```

Each iteration must conditionally select between two values, creating dependencies that prevent parallel execution.

With `ILP_FIND`, comparisons run in parallel without dependencies:

```cpp
// Fast - parallel comparisons, no csel
size_t idx = ILP_FIND(auto i, 0uz, n, 4) {
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
