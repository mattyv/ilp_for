# ILP_FOR

[![CI](https://github.com/mattyv/ilp_for/actions/workflows/ci.yml/badge.svg)](https://github.com/mattyv/ilp_for/actions/workflows/ci.yml)

Compile-time loop unrolling for complex or early exit loops (i.e. contain `break`, `continue`, `return`), where compilers typically cannot unroll.
[What is ILP?](docs/ILP.md)

```cpp
#include <ilp_for.hpp>
```

---

## How It Works

The library unrolls your loop body N times, allowing the CPU to execute multiple iterations in parallel:

```cpp
// Your code
auto idx = ILP_FIND(auto i, 0, n, 4) {
    return data[i] == target;
} ILP_END;

// Conceptually expands to:
size_t i = 0;
for (; i + 4 <= n; i += 4) {
    bool r0 = data[i+0] == target;  // Parallel evaluation
    bool r1 = data[i+1] == target;
    bool r2 = data[i+2] == target;
    bool r3 = data[i+3] == target;
    if (r0) return i+0;  // Sequential check
    if (r1) return i+1;
    if (r2) return i+2;
    if (r3) return i+3;
}
for (; i < n; ++i) {             // Remainder
    if (data[i] == target) return i;
}
return n;  // Not found (sentinel)
```

The unrolled comparisons have no data dependencies, so out-of-order CPUs can execute them simultaneously while waiting for memory.

---

## Quick Start

**[View Assembly Examples](docs/EXAMPLES.md)** - Compare ILP vs hand-rolled code on Compiler Explorer

### Find First Match

```cpp
// Returns index, or end value (sentinel) if not found
auto idx = ILP_FIND(auto i, 0, data.size(), 4) {
    return data[i] == target;
} ILP_END;

if (idx != data.size()) {
    std::cout << "Found at " << idx << "\n";
}

// Range version - returns index (size() if not found)
size_t idx = ILP_FIND_RANGE(auto&& val, data, 4) {
    return val == target;  // Simple bool return
} ILP_END;

// Auto-selecting optimal N
size_t idx = ILP_FIND_RANGE_AUTO(auto&& val, data) {
    return val == target;
} ILP_END;
```

### Sum / Reduce

```cpp
// Sum with explicit N
int sum = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, n, 4) {
    return data[i];
} ILP_END_REDUCE;

// Range sum with auto-selected N
int sum = ILP_REDUCE_RANGE_AUTO(std::plus<>{}, 0, auto&& val, data) {
    return val;
} ILP_END_REDUCE;

// Min/Max
int min_val = ILP_REDUCE_RANGE(
    [](int a, int b) { return std::min(a, b); },
    INT_MAX, auto&& val, data, 4
) {
    return val;
} ILP_END_REDUCE;
```

### Loop with Break/Continue

```cpp
ILP_FOR(void, auto i, 0, n, 4) {
    if (data[i] < 0) ILP_BREAK;
    if (data[i] == 0) ILP_CONTINUE;
    process(data[i]);
} ILP_END;
```

### Loop with Return

```cpp
// Returns std::optional<int>
auto result = ILP_FOR(int, auto i, 0, n, 4) {
    if (data[i] == target) ILP_RETURN(i);
} ILP_END;
```

---

## Macro Reference

### Loop Macros

| Macro | Description | Returns |
|-------|-------------|---------|
| `ILP_FOR(void, var, start, end, N)` | Basic loop with break/continue | void |
| `ILP_FOR(type, var, start, end, N)` | Loop with return capability | `std::optional<type>` |
| `ILP_FOR_RANGE(void, var, range, N)` | Range-based loop | void |
| `ILP_FOR_RANGE(type, var, range, N)` | Range loop with return | `std::optional<type>` |
| `ILP_FOR_AUTO(type, var, start, end)` | Auto-selects optimal N | depends on type |
| `ILP_FOR_RANGE_AUTO(type, var, range)` | Range with auto N | depends on type |

All loop macros end with `ILP_END`.

### Find Macros

| Macro | Returns | Description |
|-------|---------|-------------|
| `ILP_FIND(var, start, end, N)` | Index (or end if not found) | Find first match by index |
| `ILP_FIND_RANGE(var, range, N)` | Index (or size() if not found) | Find in range (simple bool return) |
| `ILP_FIND_RANGE_AUTO(var, range)` | Index (or size() if not found) | Auto-selects optimal N |
| `ILP_FIND_RANGE_IDX(var, idx, range, N)` | Iterator (or end iterator) | Find in range with index access |

### Reduce Macros

| Macro | Description |
|-------|-------------|
| `ILP_REDUCE(op, init, var, start, end, N)` | Index-based reduce with custom operation |
| `ILP_REDUCE_RANGE(op, init, var, range, N)` | Range-based reduce |
| `ILP_REDUCE_AUTO(op, init, var, start, end)` | Auto-selects optimal N |
| `ILP_REDUCE_RANGE_AUTO(op, init, var, range)` | Auto-selects optimal N |

### Control Flow

| Macro | Use In | Description |
|-------|--------|-------------|
| `ILP_CONTINUE` | Any loop | Skip to next iteration |
| `ILP_BREAK` | Loops | Exit loop |
| `ILP_REDUCE_RETURN(val)` | Reduce | Return value (fast path, uses SIMD) |
| `ILP_REDUCE_BREAK` | Reduce | Exit early from reduction |
| `ILP_REDUCE_BREAK_VALUE(val)` | Reduce | Return value (use with `ILP_REDUCE_BREAK`) |
| `ILP_RETURN(val)` | `*_RET` loops | Return from enclosing function |

---

## Important Notes

### Use `auto&&` for Range Loops

For range-based macros, always use `auto&&` to avoid copying each element:

```cpp
// CORRECT - Uses forwarding reference (zero copies)
ILP_REDUCE_RANGE(std::plus<>{}, 0, auto&& val, data, 4) {
    return val;
} ILP_END_REDUCE;

// WRONG - Copies each element into 'val' (slow for large types!)
ILP_REDUCE_RANGE(std::plus<>{}, 0, auto val, data, 4) {
    return val;
} ILP_END_REDUCE;
```

This matters because range loops iterate over container elements directly. Using `auto` creates a copy of each element, while `auto&&` binds to the element in-place. For a `std::vector<std::string>`, using `auto` would copy every string!

For index-based macros, use `auto` since indices are just integers:

```cpp
ILP_FOR(void, auto i, 0, n, 4) { ... }  // 'i' is an int, copying is trivial
```

### ILP_FIND Returns Index, Not Optional

`ILP_FIND` returns the index directly. If not found, it returns the end value (sentinel):

```cpp
auto idx = ILP_FIND(auto i, 0, n, 4) {
    return data[i] == target;
} ILP_END;

if (idx != n) {  // Found
    std::cout << "Found at " << idx << "\n";
}
```

### ILP_REDUCE_BREAK for Early Exit

Use `ILP_REDUCE_BREAK` with `ILP_REDUCE_BREAK_VALUE` to exit early from a reduction:

```cpp
int sum = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, n, 4) {
    if (data[i] < 0) ILP_REDUCE_BREAK;  // Stop reduction
    ILP_REDUCE_BREAK_VALUE(data[i]);    // Must use with BREAK
} ILP_END_REDUCE;
```

For simple reductions without early exit, use `ILP_REDUCE_RETURN` (fast path):

```cpp
int sum = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, n, 4) {
    ILP_REDUCE_RETURN(data[i]);  // Uses transform_reduce (SIMD)
} ILP_END_REDUCE;
```

### Associative Operations Only

Use only associative operations: `+`, `*`, `min`, `max`, `&`, `|`, `^`

**Floating-point note:** IEEE floating-point is not strictly associative due to rounding. Parallel reduction may yield results differing by a few ULPs from sequential evaluation.

### Init Values

For custom operations, `init` must be the identity element:

```cpp
// For addition, init = 0
ILP_REDUCE(std::plus<>{}, 0, ...)

// For min, init = INT_MAX
ILP_REDUCE([](int a, int b) { return std::min(a,b); }, INT_MAX, ...)

// For multiplication, init = 1
ILP_REDUCE(std::multiplies<>{}, 1, ...)
```

---

## When to Use ILP

**Use ILP for:**
- Early-exit operations (find, any-of, all-of)
- Parallel comparisons (min, max)
- Loops with break/return that compilers can't unroll

**Skip ILP for:**
- Simple sums without early exit - compilers auto-vectorize these better

```cpp
// Use ILP - early exit benefits from parallel evaluation
auto idx = ILP_FIND(auto i, 0, n, 4) {
    return data[i] == target;
} ILP_END;

// Skip ILP - compiler auto-vectorizes better
int sum = std::accumulate(data.begin(), data.end(), 0);
```

See [docs/PERFORMANCE.md](docs/PERFORMANCE.md) for benchmarks.

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

**[View Coverage Report](https://htmlpreview.github.io/?https://github.com/mattyv/ilp_for/blob/main/coverage/index.html)**

All core functionality including cleanup loops, bitwise operations, and edge cases are fully tested.

---

## Requirements

- C++23
- Header-only

---

## License

[Boost Software License 1.0](LICENSE)
