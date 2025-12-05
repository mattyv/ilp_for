# Performance Notes

This library's goal is not to be a performance library, but to ensure performance doesn't stop you from using it. The patterns here generate efficient code comparable to hand-written unrolled loops.

## Benchmarks

Apple M2, Clang 19, 10M elements, `-O3 -march=native`

| Operation | std | ILP | Speedup |
|-----------|-----|-----|---------|
| Min | 3.47ms | 0.58ms | **6.0x** |
| Any-of | 3.96ms | 1.86ms | **2.1x** |
| Sum with break | 1.80ms | 1.11ms | **1.6x** |
| Find | 1.86ms | 1.86ms | ~1.0x |

### Simple Reductions

| Operation | Handrolled | ILP | Result |
|-----------|------------|-----|--------|
| Sum | 0.75ms | 0.75ms | **equivalent** |

Modern compilers auto-vectorize simple sums effectively. ILP matches hand-optimized code.

## Early Return Performance

The `*_RET_SIMPLE` functions auto-detect the optimal mode based on return type.

### Bool Mode (Fastest)

Return `bool` to get the iterator to the first match. This avoids `csel` (conditional select) dependencies:

```cpp
// Matches std::find performance
auto it = ilp::find_range_auto(data, [&](auto&& val) {
    return val == target;  // returns bool
});
// Returns: iterator - points to match, or end() if not found
```

### Optional Mode (General Purpose)

Return early using `ILP_RETURN` (returns from enclosing function):

```cpp
std::optional<int> find_expensive() {
    ILP_FOR(int, auto i, 0uz, data.size(), 4) {
        if (expensive_check(data[i])) {
            ILP_RETURN(compute(data[i]));  // Returns from find_expensive()
        }
    } ILP_END;
    return std::nullopt;  // Not found
}
```

### Why Bool Mode is Faster

When using `ILP_FOR` with return type, the compiler generates `csel` instructions:

```cpp
// Slower - generates csel dependency chain
ILP_FOR(size_t, auto i, 0uz, n, 4) {
    if (data[i] == target) ILP_RETURN(i);
} ILP_END;
```

Each iteration must conditionally select between two values, creating dependencies that prevent parallel execution.

With `ilp::find`, comparisons run in parallel without dependencies:

```cpp
// Fast - parallel comparisons, no csel
size_t idx = ilp::find<4>(0uz, n, [&](auto i, auto) {
    return data[i] == target;
});
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
