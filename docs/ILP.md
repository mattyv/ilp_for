# Instruction-Level Parallelism (ILP)

A modern CPU core is **superscalar**: it can issue and execute several instructions in the same cycle — but only when those instructions don't depend on each other's results. High-performance loop code is largely the art of giving the core enough independent work to fill those slots.

## Skylake Microarchitecture

![Skylake Block Diagram](https://en.wikichip.org/w/images/7/7e/skylake_block_diagram.svg)

*Source: [WikiChip](https://en.wikichip.org/wiki/intel/microarchitectures/skylake_(client))*

## Key Concepts

### Execution Ports

A Skylake core has **8 execution ports** that can operate in parallel:

| Port | Operations |
|------|------------|
| 0 | ALU, Vector ALU, FMA, DIV |
| 1 | ALU, Vector ALU, FMA |
| 2 | Load, Store AGU |
| 3 | Load, Store AGU |
| 4 | Store Data |
| 5 | ALU, Vector ALU, Shuffle |
| 6 | ALU, Branch |
| 7 | Store AGU |

### The Problem: Data Dependencies

```cpp
// Sequential - each add waits for the previous result
int sum = 0;
for (int i = 0; i < n; i++) {
    sum += data[i];  // Must wait for previous sum
}
```

Every add needs the previous add's result before it can start, so the loop runs at the *latency* of the add chain — one result per add-latency — while the core sits on execution units that could be running several adds per cycle. This is a **loop-carried dependency**, and it caps throughput no matter how wide the CPU is.

### The Solution: Multiple Accumulators

```cpp
// Parallel - 4 independent chains
int sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
for (int i = 0; i < n; i += 4) {
    sum0 += data[i];      // Independent
    sum1 += data[i + 1];  // Independent
    sum2 += data[i + 2];  // Independent
    sum3 += data[i + 3];  // Independent
}
int sum = sum0 + sum1 + sum2 + sum3;
```

The four chains are independent, so the core can keep all four adds in flight at once — roughly 4x the throughput from the same execution units.

## Pipeline Latency vs Throughput

| Operation | Latency (cycles) | Throughput (per cycle) |
|-----------|------------------|------------------------|
| Integer ADD | 1 | 4 |
| FP ADD | 4 | 2 |
| FP MUL | 4 | 2 |
| FP FMA | 4 | 2 |

**Latency** = cycles from starting an operation until its result is usable
**Throughput** = how many independent operations can start per cycle

The two multiply into the unroll factor: with 4-cycle latency and 2-per-cycle throughput, the FP units only saturate with **8 independent operations** in flight — which is exactly where the library's `optimal_N` values come from.

## How ILP_FOR Works

The library unrolls loops by factor N, enabling the compiler to vectorize or interleave operations:

```cpp
// You write:
ILP_FOR(auto i, 0uz, n, 4) {
    dst[i] = src[i] * 2.0f;
} ILP_END;

// Library generates (conceptually):
for (i = 0; i + 4 <= n; i += 4) {
    dst[i]   = src[i]   * 2.0f;  // Independent
    dst[i+1] = src[i+1] * 2.0f;  // Independent
    dst[i+2] = src[i+2] * 2.0f;  // Independent
    dst[i+3] = src[i+3] * 2.0f;  // Independent
}
// + remainder loop
```

For **copy** and **transform** patterns every iteration is independent, so the compiler is free to vectorize the block or interleave its operations.

**Note:** `ILP_FOR` earns its keep on loops with early exit (`break`, `return`), where the auto-vectorizer can't help. For pure accumulation with no early exit, a plain loop usually auto-vectorizes just as well — see the README's [When to Use ILP](../README.md#when-to-use-ilp).

## Further Reading

- [Agner Fog's Optimization Manuals](https://www.agner.org/optimize/)
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html)
- [uops.info](https://uops.info/) - Instruction latency tables
