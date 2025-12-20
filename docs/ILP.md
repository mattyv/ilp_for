# Instruction-Level Parallelism (ILP)

Modern CPUs execute multiple instructions simultaneously through **superscalar execution**. Understanding this is key to writing high-performance code.

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

The CPU can only issue one add per ~3-4 cycles because each iteration depends on the previous result. This is called a **loop-carried dependency**.

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

Now the CPU can execute all 4 adds in parallel, achieving ~4x throughput.

## Pipeline Latency vs Throughput

| Operation | Latency (cycles) | Throughput (per cycle) |
|-----------|------------------|------------------------|
| Integer ADD | 1 | 4 |
| FP ADD | 4 | 2 |
| FP MUL | 4 | 2 |
| FP FMA | 4 | 2 |

**Latency** = cycles until result is ready
**Throughput** = operations per cycle when independent

With 4-cycle latency and 2/cycle throughput, you need **8 independent operations** in flight to saturate the FP units.

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

For **copy** and **transform** patterns, each iteration is independent—the compiler can vectorize or execute them in parallel.

For **accumulation** patterns with a single variable (`sum += x`), you must use multiple accumulators manually to break the dependency chain:

```cpp
float sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
ILP_FOR(auto i, 0uz, n, 4) {
    sum0 += data[i];
    sum1 += data[i + 1];
    sum2 += data[i + 2];
    sum3 += data[i + 3];
    i += 3;  // ILP_FOR increments by 1, we handle 4
} ILP_END;
float sum = sum0 + sum1 + sum2 + sum3;
```

## Further Reading

- [Agner Fog's Optimization Manuals](https://www.agner.org/optimize/)
- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html)
- [uops.info](https://uops.info/) - Instruction latency tables
