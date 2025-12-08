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
ILP_FOR(auto i, 0, n, 4) {
    if (data[i] == target) ILP_BREAK;
    process(data[i]);
} ILP_END;

// Conceptually expands to:
size_t i = 0;
for (; i + 4 <= n; i += 4) {
    bool b0 = data[i+0] == target;  // Parallel evaluation
    bool b1 = data[i+1] == target;
    bool b2 = data[i+2] == target;
    bool b3 = data[i+3] == target;
    if (b0) break;                  // Sequential check
    process(data[i+0]);
    if (b1) break;
    process(data[i+1]);
    if (b2) break;
    process(data[i+2]);
    if (b3) break;
    process(data[i+3]);
}
for (; i < n; ++i) {                // Remainder
    if (data[i] == target) break;
    process(data[i]);
}
```

The unrolled comparisons have no data dependencies, so out-of-order CPUs can execute them simultaneously while waiting for memory.

---

## Quick Start

**[View Assembly Examples](docs/EXAMPLES.md)** - Compare ILP vs hand-rolled code on Compiler Explorer

### Loop with Break/Continue

```cpp
ILP_FOR(auto i, 0, n, 4) {
    if (data[i] < 0) ILP_BREAK;
    if (data[i] == 0) ILP_CONTINUE;
    process(data[i]);
} ILP_END;
```

### Loop with Return

```cpp
// ILP_RETURN(x) returns from enclosing function
int find_index(const std::vector<int>& data, int target) {
    ILP_FOR(auto i, 0, static_cast<int>(data.size()), 4) {
        if (data[i] == target) ILP_RETURN(i);  // Returns from find_index()
    } ILP_END_RETURN;  // Must use ILP_END_RETURN when ILP_RETURN is used
    return -1;  // Not found
}
```

### Function API (Alternative)

For `std::`-style functions with early exit support:

```cpp
// Find - returns index (or end if not found)
auto idx = ilp::find<4>(0, data.size(), [&](auto i, auto) {
    return data[i] == target;
});

// Reduce with early exit via std::optional
int sum = ilp::reduce<4>(size_t{0}, data.size(), 0, std::plus<>{},
    [&](auto i) -> std::optional<int> {
        if (data[i] < 0) return std::nullopt;  // Stop
        return data[i];
    });

// Parallel min (breaks dependency chain)
int min_val = ilp::reduce_range<4>(
    data,
    std::numeric_limits<int>::max(),
    [](int a, int b) { return std::min(a, b); },
    [&](auto&& val) { return val; }
);
```

---

## API Reference

### Loop Macros 

| Macro | Description |
|-------|-------------|
| `ILP_FOR(var, start, end, N)` | Index loop with explicit N |
| `ILP_FOR_RANGE(var, range, N)` | Range-based loop with explicit N |
| `ILP_FOR_AUTO(var, start, end)` | Index loop with auto-selected N |
| `ILP_FOR_RANGE_AUTO(var, range)` | Range loop with auto-selected N |

End with `ILP_END`, or `ILP_END_RETURN` when using `ILP_RETURN`.

### Control Flow

| Macro/Function | Use In | Description |
|----------------|--------|-------------|
| `ILP_CONTINUE` | Any loop | Skip to next iteration |
| `ILP_BREAK` | Loops | Exit loop |
| `ILP_RETURN(val)` | Loops with return type | Return `val` from enclosing function |
| `-> std::optional<T>` | Reduce body | Return type for early exit support |
| `return std::nullopt` | Reduce body | Exit early from reduction |

### Function API (Alternative)

| Function | Description |
|----------|-------------|
| `ilp::find<N>(start, end, body)` | Find first match, returns index |
| `ilp::find_range<N>(range, body)` | Find in range, returns iterator |
| `ilp::reduce<N>(start, end, init, op, body)` | Reduce with optional early exit |
| `ilp::reduce_range<N>(range, init, op, body)` | Range reduce |

All have `_auto` variants that select optimal N automatically.

---

## Important Notes

### Use `auto&&` for Range Loops

For range-based functions, always use `auto&&` to avoid copying each element:

```cpp
// CORRECT - Uses forwarding reference (zero copies)
auto sum = ilp::reduce_range<4>(data, 0, std::plus<>{}, [&](auto&& val) {
    return val;
});

// WRONG - Copies each element into 'val' (slow for large types!)
auto sum = ilp::reduce_range<4>(data, 0, std::plus<>{}, [&](auto val) {
    return val;
});
```

This matters because range loops iterate over container elements directly. Using `auto` creates a copy of each element, while `auto&&` binds to the element in-place. For a `std::vector<std::string>`, using `auto` would copy every string!

For index-based functions, use `auto` since indices are just integers:

```cpp
ilp::reduce<4>(0, n, 0, std::plus<>{}, [&](auto i) { return data[i]; });
```

### Find Returns Index or Iterator

`ilp::find` returns the index directly. If not found, it returns the end value (sentinel):

```cpp
auto idx = ilp::find<4>(0, n, [&](auto i, auto) {
    return data[i] == target;
});

if (idx != n) {  // Found
    std::cout << "Found at " << idx << "\n";
}
```

Range versions return iterators:

```cpp
auto it = ilp::find_range<4>(data, [&](auto&& val) {
    return val == target;
});

if (it != data.end()) {
    std::cout << "Found: " << *it << "\n";
}
```

### Reduce with Early Exit

Use `std::optional` return type with `std::nullopt` to exit early from a reduction:

```cpp
int sum = ilp::reduce<4>(size_t{0}, data.size(), 0, std::plus<>{},
    [&](auto i) -> std::optional<int> {
        if (data[i] < 0) return std::nullopt;  // Stop reduction
        return data[i];
    });
```

For simple reductions without early exit, just return the value directly:

```cpp
int sum = ilp::reduce<4>(0, n, 0, std::plus<>{}, [&](auto i) {
    return data[i];
});
```

### Associative Operations Only

Use only associative operations: `+`, `*`, `min`, `max`, `&`, `|`, `^`

**Floating-point note:** IEEE floating-point is not strictly associative due to rounding. Parallel reduction may yield results differing by a few ULPs from sequential evaluation.

### Init Values

For custom operations, `init` must be the identity element:

```cpp
// For addition, init = 0
ilp::reduce<4>(0, n, 0, std::plus<>{}, ...)

// For min, init = INT_MAX
ilp::reduce<4>(0, n, INT_MAX, [](int a, int b) { return std::min(a,b); }, ...)

// For multiplication, init = 1
ilp::reduce<4>(0, n, 1, std::multiplies<>{}, ...)
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
auto idx = ilp::find<4>(0, n, [&](auto i, auto) {
    return data[i] == target;
});

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
constexpr auto N = ilp::optimal_N<ilp::LoopType::Sum, double>;
```

Default profile values (int / float):

| LoopType | int32 | int64 | float | double |
|----------|-------|-------|-------|--------|
| Sum | 4 | 4 | 8 | 8 |
| DotProduct | - | - | 8 | 8 |
| Search | 4 | 4 | 4 | 4 |
| Multiply | 8 | 8 | 8 | 8 |
| MinMax | 4 | 4 | 8 | 8 |

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
