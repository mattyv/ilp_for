# Performance Notes

The numbers, and the reasoning behind them: where the speedup comes from, and where you shouldn't expect one. The generated code matches what you'd get hand-writing the unrolled main-loop-plus-remainder pattern yourself.

## ILP_FOR Benchmarks

Apple M2, Clang 19, 10M elements, `-O3 -march=native`

| Loop Type | Simple | Pragma | ILP | Speedup |
|-----------|--------|--------|-----|---------|
| `ILP_FOR` with `ILP_BREAK` | 1.46ms | 1.46ms | 0.94ms | **1.56x** |
| `ILP_FOR` with `ILP_RETURN` | 1.68ms | 1.51ms | 0.94ms | **1.79x** |
| `ILP_FOR` with `ILP_CONTINUE` | 1.96ms | 1.72ms | 1.49ms | **1.31x** |
| `ILP_FOR_RANGE` with `ILP_BREAK` | 2.21ms | - | 0.94ms | **2.35x** |

Note: for early-exit patterns, `#pragma unroll` performs no better than the simple loop — the per-iteration bounds checks it inserts cancel the unroll. See [Why Not Pragma Unroll?](PRAGMA_UNROLL.md).

### Why ILP_RETURN is Faster

`ILP_RETURN` lets the compiler hoist the comparisons ahead of the conditional return logic, so they execute in parallel:

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
- Bodies with loop-carried dependency chains (min, max, running products)
- Searches whose comparisons can be evaluated in parallel

**Skip ILP for:**
- Straight-line sums with no early exit — the auto-vectorizer already wins
- Anything the compiler demonstrably optimizes well on its own

```cpp
// Use ILP - early exit benefits from parallel evaluation
ILP_FOR(auto i, 0, n, 4) {
    if (expensive_check(data[i])) ILP_RETURN(result);
} ILP_END_RETURN;

// Skip ILP - compiler auto-vectorizes better
int sum = std::accumulate(data.begin(), data.end(), 0);
```

## Why Not `#pragma unroll`?

For loops with early exit, the compiler cannot determine the trip count (SCEV has no answer for a loop that might `break`), so `#pragma unroll` conservatively re-checks the bounds after every element — and those checks consume exactly the win the unroll was meant to deliver.

**Result:** for early-exit patterns, pragma unroll performs the same as a simple loop.

See [PRAGMA_UNROLL.md](PRAGMA_UNROLL.md) for assembly evidence and technical details.
