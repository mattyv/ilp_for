# ILP_FOR

[![CI](https://github.com/mattyv/ilp_for/actions/workflows/ci.yml/badge.svg)](https://github.com/mattyv/ilp_for/actions/workflows/ci.yml)

Compile-time loop unrolling for complex or early exit loops (i.e. contian `break`, `continue`, `return`), where compilers typically cannot unroll.
[What is ILP?](docs/ILP.md)

```cpp
#include <ilp_for.hpp>
```

---

## How It Works

The library unrolls your loop body N times, allowing the CPU to execute multiple iterations in parallel:

```cpp
// Your code
ILP_FOR_RET(int, auto i, 0, n, 4) {
    if (data[i] == target) ILP_RETURN(i);
} ILP_END_RET;

// Conceptually expands to:
for (int i = 0; i + 4 <= n; i += 4) {
    if (data[i+0] == target) return i+0;
    if (data[i+1] == target) return i+1;
    if (data[i+2] == target) return i+2;
    if (data[i+3] == target) return i+3;
}
for (int i = /*remainder*/; i < n; i++) {  // cleanup loop
    if (data[i] == target) return i;
}
```

The unrolled comparisons have no data dependencies, so out-of-order CPUs can execute them simultaneously while waiting for memory.

---

## Quick Start

**[View Assembly Examples →](docs/EXAMPLES.md)** - Compare ILP vs hand-rolled code on Compiler Explorer

Use `*_AUTO` macros first - they select optimal unroll factors for your CPU:

```cpp
// Find element
auto idx = ILP_FOR_UNTIL_RANGE_AUTO(auto&& val, data) {
    return val == target;
} ILP_END_UNTIL;
// Returns std::optional<size_t> - index if found

// Sum
auto sum = ILP_REDUCE_RANGE_SUM_AUTO(auto&& val, data) {
    return val;
} ILP_END_REDUCE;

// Min
auto min_val = ILP_REDUCE_RANGE_AUTO(
    [](auto a, auto b) { return a < b ? a : b; },
    INT_MAX, auto&& val, data
) {
    return val;
} ILP_END_REDUCE;
```

Need more control? Specify N manually:

```cpp
ILP_FOR_RET(int, auto i, 0, n, 8) {
    if (data[i] == target) ILP_RETURN(i);
} ILP_END_RET;
```

### ⚠️ Important: Use `auto&&` for Range Loops

**For range-based macros**, always use `auto&&` for the loop variable to avoid copying:

```cpp
// ✅ CORRECT - Uses reference (fast)
ILP_REDUCE_RANGE_SUM_AUTO(auto&& val, data) { return val; } ILP_END_REDUCE;

// ❌ WRONG - Copies each element (slow!)
ILP_REDUCE_RANGE_SUM_AUTO(auto val, data) { return val; } ILP_END_REDUCE;
```

**For index-based macros**, use `auto` since indices are trivial to copy:

```cpp
// ✅ CORRECT - Index is just an integer
ILP_FOR_RET_SIMPLE_AUTO(auto i, 0, data.size()) { ... } ILP_END;
```

The library will issue a deprecation warning if you accidentally use `auto` (by-value) with range macros.

---

## Understanding the Variants

| Suffix | Control Flow | Use When |
|--------|-------------|----------|
| `*_SIMPLE` | None | No break/return needed - tightest codegen |
| (none) | `break`, `continue` | Need to exit loop early |
| `*_RET` | `return` from function | Need to return value from enclosing function |

**Why SIMPLE?** When the compiler knows there's no early exit, it generates tighter code. Use SIMPLE when you don't need control flow.

---

## AUTO Macros (Recommended)

These macros use CPU profiles defined in the header to select optimal unroll factors based on loop type and element size. Defaults to conservative values; set `ILP_CPU` at compile time to match your target architecture (see [Advanced](#cpu-architecture)).

**Variable declarations:** You must specify the type in the macro parameter (e.g., `auto i`, `auto&& val`). The variable is defined by the macro - don't declare it beforehand. Use `auto&&` for range elements, `auto` for indices.

### Loop Macros

| Macro | Returns | Description |
|-------|---------|-------------|
| `ILP_FOR_UNTIL_RANGE_AUTO(var, range)` | `optional<size_t>` | Find first match (bool return) |
| `ILP_FOR_UNTIL_AUTO(i, start, end)` | `optional<T>` | Index-based find |
| `ILP_FOR_RET_SIMPLE_AUTO(i, start, end)` | `optional<T>` | Loop with early return |
| `ILP_FOR_RANGE_IDX_RET_SIMPLE_AUTO(val, idx, range)` | `optional<T>` | Range with index access |

### Reduce Macros

| Macro | Description |
|-------|-------------|
| `ILP_REDUCE_RANGE_SUM_AUTO(var, range)` | Sum over range |
| `ILP_REDUCE_SUM_AUTO(i, start, end)` | Index-based sum |
| `ILP_REDUCE_RANGE_AUTO(op, init, var, range)` | Custom reduce over range |
| `ILP_REDUCE_AUTO(op, init, i, start, end)` | Index-based custom reduce |

---

## Manual N Macros

For when you need specific unroll factors:

### Loop Macros

```cpp
// Simple - no control flow
ILP_FOR(i, 0, n, 4) {
    data[i] = data[i] * 2;
} ILP_END;

// With break/continue
ILP_FOR(i, 0, n, 4) {
    if (data[i] < 0) ILP_BREAK;
    if (data[i] == 0) ILP_CONTINUE;
    process(data[i]);
} ILP_END;

// With return
ILP_FOR_RET(int, i, 0, n, 4) {
    if (data[i] == target) ILP_RETURN(i);
} ILP_END_RET;

// Range-based
ILP_FOR_RANGE(val, data, 4) {
    process(val);
} ILP_END;

// Step loop
ILP_FOR_STEP(i, 0, n, 2, 4) {
    data[i] = data[i] * 2;  // every 2nd element
} ILP_END;
```

### Reduce Macros

```cpp
// Sum
int sum = ILP_REDUCE_RANGE_SUM(val, data, 4) {
    return val;
} ILP_END_REDUCE;

// Custom operation
int min_val = ILP_REDUCE_RANGE(
    [](int a, int b) { return std::min(a, b); },
    INT_MAX, val, data, 4
) {
    return val;
} ILP_END_REDUCE;

// With break
int sum = ILP_REDUCE(std::plus<>{}, 0, i, 0, n, 4) {
    if (i >= stop_at) ILP_BREAK_RET(0);
    return data[i];
} ILP_END_REDUCE;
```

---

## Control Flow

| Macro | Use In | Description |
|-------|--------|-------------|
| `ILP_CONTINUE` | Any loop | Skip to next iteration |
| `ILP_BREAK` | Loops | Exit loop |
| `ILP_BREAK_RET(val)` | Reduce | Exit and return value |
| `ILP_RETURN(val)` | `*_RET` loops | Exit loop and return from enclosing function |

### Loop Endings

| Macro | Use With |
|-------|----------|
| `ILP_END` | All loops without return |
| `ILP_END_RET` | `ILP_FOR_RET` |
| `ILP_END_REDUCE` | All reduce macros |
| `ILP_END_UNTIL` | `ILP_FOR_UNTIL*` macros |

---

## When to Use ILP

The goal is not to be a performance library, but to ensure performance doesn't stop you from using it.

**Use ILP for:**
- Early-exit operations (find, any-of)
- Parallel comparisons (min, max)
- Loops with break/return

**Skip ILP for:**
- Simple sums without early exit - compilers auto-vectorize these better

```cpp
// Use ILP - early exit benefits from multi-accumulator
auto idx = ILP_FOR_UNTIL_RANGE_AUTO(val, data) {
    return val == target;
} ILP_END_UNTIL;

// Skip ILP - compiler auto-vectorizes better
int sum = std::accumulate(data.begin(), data.end(), 0);
```

See [docs/PERFORMANCE.md](docs/PERFORMANCE.md) for benchmarks and detailed analysis.

---

## Important Notes

### Return Type Overflow

The return type is inferred from your lambda. Cast to larger types for big sums (compiler will warn about potential overflow):

```cpp
// Overflow risk with int (compiler warning)
auto result = ILP_REDUCE_RANGE_SUM(val, data, 4) {
    return val;
} ILP_END_REDUCE;

// Safe
auto result = ILP_REDUCE_RANGE_SUM(val, data, 4) {
    return static_cast<int64_t>(val);
} ILP_END_REDUCE;
```

### Non-Associative Operations

Only use associative operations: `+`, `*`, `min`, `max`, `&`, `|`, `^`

```cpp
// Undefined - subtraction is not associative
ILP_REDUCE_RANGE(std::minus<>{}, 100, val, data, 4) { ... }
```

**Floating-point note:** IEEE floating-point arithmetic is not strictly associative due to rounding. Parallel reduction may yield results differing by a few ULPs from sequential evaluation. This is inherent to all parallel/unrolled reductions and typically negligible for well-conditioned data.

### ILP_FOR_UNTIL Return Type

Returns `std::optional<T>` (index), not `bool`:

```cpp
auto result = ILP_FOR_UNTIL_RANGE_AUTO(val, data) {
    return val == target;
} ILP_END_UNTIL;

if (result) {
    std::cout << "Found at " << *result << "\n";
}
```

### Init Values for Custom Lambdas

For custom operations, `init` must be the identity element:

```cpp
// For addition lambda, init must be 0
auto result = ILP_REDUCE_RANGE(
    [](int a, int b) { return a + b; },
    0,  // identity for +
    val, data, 4
) { return val; } ILP_END_REDUCE;

// To add offset, do it after
auto result = 100 + ILP_REDUCE_RANGE(...);
```

---

## Advanced

### CPU Architecture

```bash
clang++ -std=c++23 -DILP_CPU=skylake    # Intel Skylake
clang++ -std=c++23 -DILP_CPU=apple_m1   # Apple M1
clang++ -std=c++23 -DILP_CPU=zen5       # AMD Zen 5
```

### Debugging

Disable ILP for easier debugging:

```bash
clang++ -std=c++23 -DILP_MODE_SIMPLE -O0 -g mycode.cpp
```

All macros expand to simple `for` loops with same semantics.

### optimal_N

```cpp
constexpr auto N = ilp::optimal_N<ilp::LoopType::Sum, sizeof(double)>;
```

| LoopType | 1B | 2B | 4B | 8B |
|----------|----|----|----|----|
| Sum | 16 | 8 | 4 | 8 |
| DotProduct | - | - | 8 | 8 |
| Search | 8 | 4 | 4 | 4 |
| Copy | 16 | 8 | 4 | 4 |
| Transform | 8 | 4 | 4 | 4 |

### Function API

For those who prefer functions over macros, see the full API in `ilp_for.hpp`.

---

## Test Coverage

**[View Coverage Report →](https://htmlpreview.github.io/?https://github.com/mattyv/ilp_for/blob/main/coverage/index.html)**

All core functionality including cleanup loops, bitwise operations, and edge cases are fully tested.

---

## Requirements

- C++23
- Header-only

---

## License

[Boost Software License 1.0](LICENSE)
