# ilp_for

Compile-time loop unrolling macros with full control flow support (break, continue, return) for hiding instruction latency via ILP.

| Constraint | Value |
|------------|-------|
| Thread safety | none (single-threaded only) |
| Latency | hides 4-8 cycle FP latency via N parallel accumulators |
| Memory | N × sizeof(accumulator) stack overhead |
| Max capacity | N ≤ 16 (warns if exceeded) |
| C++ standard | C++23 required |

---

## Quick Start

```cpp
#include "ilp_for.hpp"
#include <vector>

int64_t sum(const std::vector<int>& v) {
    return ILP_REDUCE_RANGE_AUTO(std::plus<>{}, 0LL, auto&& x, v) {
        return static_cast<int64_t>(x);
    } ILP_END_REDUCE;
}
```

---

## Types

### LoopCtrl<R>

```cpp
// R = void (default)
struct LoopCtrl<void> {
    bool ok;  // false after ILP_BREAK
};

// R = return type
struct LoopCtrl<R> {
    bool ok;
    std::optional<R> return_value;  // set by ILP_RETURN
};
```

### LoopType

```cpp
enum class LoopType {
    Sum,         // acc += val
    DotProduct,  // acc += a * b
    Search,      // find with early exit
    Copy,        // dst = src
    Transform,   // dst = f(src)
    Multiply,    // acc *= val
    Divide,      // val / const
    Sqrt,        // sqrt(val)
    MinMax,      // acc = min/max(acc, val)
    Bitwise,     // acc &= val, |=, ^=
    Shift        // val << n, val >> n
};
```

### optimal_N

```cpp
template<LoopType L, typename T>
inline constexpr std::size_t optimal_N;
// e.g., optimal_N<LoopType::Sum, double> == 8
```

---

## API

### ILP_FOR(loop_var_decl, start, end, N)

Unrolled loop over index range `[start, end)`.

```cpp
std::vector<int> data(100);
int sum = 0;
ILP_FOR(auto i, 0, 100, 4) {
    sum += data[i];
} ILP_END;
```

**Pitfalls:**
- Missing `ILP_END` causes cryptic compile error mentioning `For_Context_USE_ILP_END`
- `start` and `end` must be same integral type
- Body captures by reference; dangling refs if data goes out of scope

---

### ILP_FOR with ILP_BREAK / ILP_CONTINUE

Control flow within loops.

```cpp
std::vector<int> data = {1, 2, -1, 4, 5};
int sum = 0;
ILP_FOR(auto i, 0uz, data.size(), 4) {
    if (data[i] < 0) ILP_BREAK;     // exits loop
    if (data[i] == 0) ILP_CONTINUE; // skips this iteration
    sum += data[i];
} ILP_END;
// sum == 3
```

**Pitfalls:**
- `ILP_BREAK` may process additional elements in same unroll block before exiting
- Using `break` or `continue` keywords causes compile error (must use macros)

---

### ILP_FOR_RET(ret_type, loop_var_decl, start, end, N)

Loop that can return from enclosing function via `ILP_RETURN(x)`.

```cpp
int find_index(const std::vector<int>& v, int target) {
    ILP_FOR_RET(int, auto i, 0, static_cast<int>(v.size()), 4) {
        if (v[i] == target) ILP_RETURN(i);
    } ILP_END_RET;
    return -1;  // not found
}
```

**Pitfalls:**
- Must use `ILP_END_RET` (not `ILP_END`)
- `ret_type` must match enclosing function return type exactly
- Mixing `ILP_END_RET` with `ILP_FOR` causes wrong context error

---

### ILP_FOR_RANGE(loop_var_decl, range, N)

Range-based loop with control flow support.

```cpp
std::vector<int> data = {1, 2, 3, 4, 5};
int sum = 0;
ILP_FOR_RANGE(auto&& x, data, 4) {
    sum += x;
} ILP_END;
```

**Pitfalls:**
- Range must satisfy `std::ranges::random_access_range`
- Non-contiguous ranges (e.g., `std::deque`) have slower iteration

---

### ILP_FIND(loop_var_decl, start, end, N)

Speculative parallel search returning first matching index.

```cpp
std::vector<int> data(1000, 0);
data[500] = 42;
size_t idx = ILP_FIND(auto i, 0uz, data.size(), 4) {
    return data[i] == 42;  // return bool
} ILP_END;
// idx == 500
// idx == data.size() if not found
```

**Pitfalls:**
- Body executes speculatively; side effects in body may occur past match
- Returns `end` (not -1 or optional) when not found
- Lambda must return `bool`, index type, or `std::optional<T>`

---

### ILP_FIND_RANGE(loop_var_decl, range, N)

Range-based find returning iterator.

```cpp
std::vector<int> data = {1, 2, 3, 4, 5};
auto it = ILP_FIND_RANGE(auto&& x, data, 4) {
    return x > 3;
} ILP_END;
// *it == 4
// it == data.end() if not found
```

**Pitfalls:**
- Returns `end()` iterator, not `std::nullopt`
- Must dereference carefully if iterator may be end

---

### ILP_REDUCE(op, init, loop_var_decl, start, end, N)

Multi-accumulator reduction for hiding FP latency.

```cpp
std::vector<double> data(1000);
double sum = ILP_REDUCE(std::plus<>{}, 0.0, auto i, 0uz, data.size(), 8) {
    return data[i];
} ILP_END_REDUCE;
```

**Pitfalls:**
- Must use `ILP_END_REDUCE` (not `ILP_END`)
- `op` must be associative; non-associative ops give wrong results
- FP addition is not truly associative; results may differ from sequential
- `init` type determines accumulator type; use `0.0` not `0` for double

---

### ILP_REDUCE with ILP_REDUCE_RETURN / ILP_REDUCE_BREAK

Reduction with control flow.

```cpp
std::vector<int> data = {1, 2, 3, -1, 5};  // -1 is sentinel
int sum = ILP_REDUCE(std::plus<>{}, 0, auto i, 0uz, data.size(), 4) {
    if (data[i] < 0) ILP_REDUCE_BREAK;
    ILP_REDUCE_RETURN(data[i]);
} ILP_END_REDUCE;
// sum == 6
```

**Pitfalls:**
- Cannot use plain `return x;` - must use `ILP_REDUCE_RETURN(x)`
- `ILP_REDUCE_BREAK` breaks by index, not accumulated value (parallel accumulators)
- Partial unroll block may contribute before break detected

---

### ILP_REDUCE_RANGE(op, init, loop_var_decl, range, N)

Range-based reduction.

```cpp
std::vector<int> data = {1, 2, 3, 4, 5};
int sum = ILP_REDUCE_RANGE(std::plus<>{}, 0, auto&& x, data, 4) {
    return x;
} ILP_END_REDUCE;
```

**Pitfalls:**
- Contiguous ranges dispatch to `std::transform_reduce` for SIMD
- Non-contiguous ranges use manual multi-accumulator pattern

---

### ILP_REDUCE_RANGE_AUTO(op, init, loop_var_decl, range)

Auto-selects optimal N based on CPU profile and element type.

```cpp
std::vector<double> data(1000);
double sum = ILP_REDUCE_RANGE_AUTO(std::plus<>{}, 0.0, auto&& x, data) {
    return x;
} ILP_END_REDUCE;
// Uses optimal_N<LoopType::Sum, double> automatically
```

**Pitfalls:**
- Default CPU profile is conservative; define `ILP_CPU` for specific arch
- Available profiles: `default`, `skylake`, `apple_m1`, `zen5`

---

### ilp::find_range<N>(range, predicate) -> iterator

Functional API for range-based find.

```cpp
std::vector<int> data = {1, 2, 3, 4, 5};
auto it = ilp::find_range<4>(data, [](int x) { return x > 3; });
```

**Pitfalls:**
- Predicate must return exactly `bool` (not int, not truthy)

---

### ilp::reduce_range<N>(range, init, op, body) -> R

Functional API for range-based reduction.

```cpp
std::vector<int> data = {1, 2, 3, 4, 5};
int sum = ilp::reduce_range<4>(data, 0, std::plus<>{}, [](int x) { return x; });
```

**Pitfalls:**
- Body without ctrl arg dispatches to `std::transform_reduce`
- Body with `LoopCtrl<void>&` uses manual multi-accumulator

---

## Anti-patterns

### Wrong: Using plain return in reduce body

```cpp
// Compiles but wrong - return type mismatch
ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 100, 4) {
    return i;  // Missing ILP_REDUCE_RETURN
} ILP_END_REDUCE;
```

```cpp
// Correct
ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 100, 4) {
    ILP_REDUCE_RETURN(i);
} ILP_END_REDUCE;
```

---

### Wrong: Mismatched END macro

```cpp
// Compile error: wrong context
ILP_FOR(auto i, 0, 100, 4) {
    // ...
} ILP_END_REDUCE;  // Wrong! Use ILP_END
```

```cpp
// Correct
ILP_FOR(auto i, 0, 100, 4) {
    // ...
} ILP_END;
```

---

### Wrong: Using C++ keywords for control flow

```cpp
// Compile error inside lambda
ILP_FOR(auto i, 0, 100, 4) {
    if (i == 50) break;  // Error: break outside loop
} ILP_END;
```

```cpp
// Correct
ILP_FOR(auto i, 0, 100, 4) {
    if (i == 50) ILP_BREAK;
} ILP_END;
```

---

### Wrong: Assuming exact break point

```cpp
// May sum past sentinel due to unrolling
std::vector<int> data = {1, 2, 3, -1, 5, 6, 7, 8};
int sum = 0;
ILP_FOR(auto i, 0uz, data.size(), 4) {
    if (data[i] < 0) ILP_BREAK;
    sum += data[i];  // Elements 4,5,6,7 may execute before break detected
} ILP_END;
```

```cpp
// Use ILP_REDUCE for correct partial sums
int sum = ILP_REDUCE(std::plus<>{}, 0, auto i, 0uz, data.size(), 4) {
    if (data[i] < 0) ILP_REDUCE_BREAK;
    ILP_REDUCE_RETURN(data[i]);
} ILP_END_REDUCE;
```

---

### Wrong: Small accumulator for large sum

```cpp
// Overflow: int accumulator with int64 elements
std::vector<int64_t> big_data(1000000, 1000000);
int sum = ILP_REDUCE_RANGE(std::plus<>{}, 0, auto&& x, big_data, 4) {
    return static_cast<int>(x);  // Truncation + overflow
} ILP_END_REDUCE;
```

```cpp
// Correct: Match accumulator to data
int64_t sum = ILP_REDUCE_RANGE(std::plus<>{}, 0LL, auto&& x, big_data, 4) {
    return x;
} ILP_END_REDUCE;
```

---

### Wrong: Non-associative operation in reduce

```cpp
// Floating-point subtraction is not associative
// (a - b) - c != a - (b - c)
double result = ILP_REDUCE(std::minus<>{}, 100.0, auto i, 0, 10, 4) {
    return 1.0;  // Undefined result due to multi-accumulator
} ILP_END_REDUCE;
```

```cpp
// Correct: Use associative operation
double result = ILP_REDUCE(std::plus<>{}, 100.0, auto i, 0, 10, 4) {
    return -1.0;  // Negate values, then add
} ILP_END_REDUCE;
```

---

### Wrong: Unroll factor too large

```cpp
// Warning: N > 16 is counterproductive
ILP_FOR(auto i, 0, 1000, 32) {  // Excessive unrolling
    // Causes instruction cache bloat
} ILP_END;
```

```cpp
// Correct: Use 4-8 for most workloads
ILP_FOR(auto i, 0, 1000, 8) {
    // ...
} ILP_END;
```

---

## Mode Selection

Define before including header:

| Mode | Define | Best for |
|------|--------|----------|
| Full ILP | (default) | Clang ccmp optimization |
| Simple | `ILP_MODE_SIMPLE` | Debugging, correctness testing |
| Pragma | `ILP_MODE_PRAGMA` | GCC auto-vectorization |

```cpp
#define ILP_MODE_SIMPLE  // Plain loops for debugging
#include "ilp_for.hpp"
```
