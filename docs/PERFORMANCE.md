# Performance Notes

This library's goal is not to be a performance library, but to ensure performance doesn't stop you from using it. The patterns here generate efficient code comparable to hand-written unrolled loops.

## ILP_FOR Benchmarks

Apple M2, Clang 19, 10M elements, `-O3 -march=native`

| Loop Type | Simple | Pragma | ILP | Speedup |
|-----------|--------|--------|-----|---------|
| `ILP_FOR` with `ILP_BREAK` | 1.46ms | 1.46ms | 0.94ms | **1.56x** |
| `ILP_FOR` with `ILP_RETURN` | 1.68ms | 1.51ms | 0.94ms | **1.79x** |
| `ILP_FOR` with `ILP_CONTINUE` | 1.96ms | 1.72ms | 1.49ms | **1.31x** |
| `ILP_FOR_RANGE` with `ILP_BREAK` | 2.21ms | - | 0.94ms | **2.35x** |

Note: `#pragma unroll` is slower than a simple loop for early-exit patterns due to per-iteration bounds checks. See [Why Not Pragma Unroll?](PRAGMA_UNROLL.md) for details.

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

For loops with early exit, `#pragma unroll` inserts per-iteration bounds checks that negate the performance benefit. The compiler cannot determine the trip count (SCEV fails for loops with `break`), so it conservatively checks bounds after each element.

**Result:** Pragma unroll performs the same as a simple loop for early-exit patterns.

See [PRAGMA_UNROLL.md](PRAGMA_UNROLL.md) for assembly evidence and technical details.
