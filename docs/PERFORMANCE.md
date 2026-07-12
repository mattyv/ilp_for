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

### x86: AMD Ryzen AI 9 HX PRO 370 (Zen 5), 10M elements, `-O3 -march=native`

Clang 20:

| Loop Type | Simple | Pragma | ILP | Speedup |
|-----------|--------|--------|-----|---------|
| `ILP_FOR` with `ILP_BREAK` | 1.02ms | 0.73ms | 0.50ms | **2.04x** |
| `ILP_FOR` with `ILP_RETURN` | 1.02ms | 0.70ms | 0.49ms | **2.07x** |
| `ILP_FOR` with `ILP_CONTINUE` | 1.10ms | 1.03ms | 1.02ms | **1.08x** |
| `ILP_FOR_RANGE` with `ILP_BREAK` | 1.18ms | - | 0.49ms | **2.40x** |

GCC 15:

| Loop Type | Simple | Pragma | ILP | Speedup |
|-----------|--------|--------|-----|---------|
| `ILP_FOR` with `ILP_BREAK` | 0.30ms | 0.32ms | 0.50ms | 0.60x |
| `ILP_FOR` with `ILP_RETURN` | 1.04ms | 0.66ms | 0.53ms | **1.95x** |
| `ILP_FOR` with `ILP_CONTINUE` | 0.34ms | 0.33ms | 13.39ms | 0.03x |
| `ILP_FOR_RANGE` with `ILP_BREAK` | 0.32ms | - | 0.47ms | 0.68x |

Two honest caveats on GCC 15 / Zen 5:

- **GCC's early-break vectorizer wins the simple loops.** GCC 14+ auto-vectorizes the plain `break`/`continue`/range loops here (~0.3ms), and the ILP transformation blocks that vectorization. On GCC, measure before reaching for ILP on these patterns; `ILP_RETURN` still delivers ~2x.
- **The `ILP_CONTINUE` benchmark hits GCC's predicate-order branch-miss cliff** (13.39ms): an unpredictable parity check ahead of a rarely-true threshold check, through the macro expansion. Order the most-selective condition first, or mark the enclosing function `ILP_FLATTEN` — see the GCC predicate-order caveat in [PRAGMA_UNROLL.md](PRAGMA_UNROLL.md).

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
