# ILP_FOR

Compile-time loop unrolling with full control flow support.

Defined in header `<ilp_for.hpp>`

---

## Synopsis

```cpp
namespace ilp {

// Control object
template<typename R = void> struct LoopCtrl;

// Loop types for optimal_N selection
enum class LoopType { Sum, DotProduct, Search, Copy, Transform };

// CPU-tuned unroll factors
template<LoopType T, std::size_t ElementBytes = 4>
inline constexpr std::size_t optimal_N;

// Index-based loops
template<std::size_t N, std::integral T, typename F>
void for_loop_simple(T start, T end, F&& body);

template<std::size_t N, std::integral T, typename F>
void for_loop(T start, T end, F&& body);

template<typename R, std::size_t N, std::integral T, typename F>
std::optional<R> for_loop_ret(T start, T end, F&& body);

// Auto-selecting loops
template<std::integral T, typename F>
void for_loop_sum(T start, T end, F&& body);

template<typename R, std::integral T, typename F>
std::optional<R> for_loop_search(T start, T end, F&& body);

} // namespace ilp
```

---

## Macros - Preferred Interfce

### ILP_FOR_SIMPLE

```cpp
#define ILP_FOR_SIMPLE(var, start, end, N) /* ... */
```

Simple loop macro without control flow.

**Example**
```cpp
int sum = 0;
ILP_FOR_SIMPLE(i, 0, 100, 4) {
    sum += data[i];
} ILP_END;
```

---

### ILP_FOR

```cpp
#define ILP_FOR(var, start, end, N) /* ... */
```

Loop macro with break/continue support.

**Example**
```cpp
ILP_FOR(i, 0, 100, 4) {
    if (data[i] < 0) ILP_BREAK;
    if (data[i] == 0) ILP_CONTINUE;
    process(data[i]);
} ILP_END;
```

---

### ILP_FOR_RET

```cpp
#define ILP_FOR_RET(ret_type, var, start, end, N) /* ... */
```

Loop macro with return value support.

**Example**
```cpp
ILP_FOR_RET(int, i, 0, 100, 4) {
    if (data[i] == target) {
        ILP_RETURN(i);
    }
} ILP_END_RET;
// Returns from enclosing function if found
```

---

## Member functions

### for_loop_simple

```cpp
template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_simple(T start, T end, F&& body);
```

Executes `body` for each value in `[start, end)` with unroll factor `N`.

**Parameters**
- `start` - beginning of range (inclusive)
- `end` - end of range (exclusive)
- `body` - callable with signature `void(T)`

**Example**
```cpp
int sum = 0;
ilp::for_loop_simple<4>(0, 100, [&](int i) {
    sum += data[i];
});
```

---

### for_loop

```cpp
template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
void for_loop(T start, T end, F&& body);
```

Executes `body` with control flow support (break/continue).

**Parameters**
- `start` - beginning of range
- `end` - end of range
- `body` - callable with signature `void(T, LoopCtrl<void>&)`

**Example**
```cpp
ilp::for_loop<4>(0, 100, [&](int i, auto& ctrl) {
    if (data[i] < 0) ctrl.break_loop();
});
```

---

### for_loop_ret

```cpp
template<typename R, std::size_t N = 4, std::integral T, typename F>
std::optional<R> for_loop_ret(T start, T end, F&& body);
```

Executes `body` with early return support.

**Parameters**
- `start` - beginning of range
- `end` - end of range
- `body` - callable with signature `void(T, LoopCtrl<R>&)`

**Return value**

`std::optional<R>` containing returned value, or `std::nullopt` if loop completed.

**Example**
```cpp
auto result = ilp::for_loop_ret<int, 4>(0, 100, [&](int i, auto& ctrl) {
    if (data[i] == target) {
        ctrl.return_with(i);
    }
});
if (result) std::cout << "Found at " << *result;
```

---

### for_loop_sum

```cpp
template<std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_sum(T start, T end, F&& body);
```

Index loop with CPU-optimized unroll factor for sum/reduction operations.

Uses `optimal_N<LoopType::Sum, sizeof(T)>`.

**Example**
```cpp
long long sum = 0;
ilp::for_loop_sum(0LL, 1000000LL, [&](long long i) {
    sum += data[i];
});
// Uses N=8 for 8-byte integers
```

---

### for_loop_search

```cpp
template<typename R, std::integral T, typename F>
std::optional<R> for_loop_search(T start, T end, F&& body);
```

Search loop with CPU-optimized unroll factor for early-exit patterns.

Uses `optimal_N<LoopType::Search, sizeof(T)>`.

**Example**
```cpp
auto idx = ilp::for_loop_search<int>(0, 100, [&](int i, auto& ctrl) {
    if (data[i] == target) ctrl.return_with(i);
});
// Uses N=4 for 4-byte integers
```

---

### for_loop_range_simple

```cpp
template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
void for_loop_range_simple(Range&& range, F&& body);
```

Range-based loop without control flow.

**Parameters**
- `range` - random access range
- `body` - callable with signature `void(range_value_t<Range>&)`

**Example**
```cpp
std::vector<int> v = {1, 2, 3, 4, 5};
int sum = 0;
ilp::for_loop_range_simple<4>(v, [&](int x) {
    sum += x;
});
```

---

## CPU Architecture Selection

Specify at compile time:

```bash
clang++ -std=c++23 -DILP_CPU=skylake    # Intel Skylake
clang++ -std=c++23 -DILP_CPU=apple_m1   # Apple M1
clang++ -std=c++23                       # Default (conservative)
```

---

## optimal_N

```cpp
template<LoopType T, std::size_t ElementBytes = 4>
inline constexpr std::size_t optimal_N;
```

Provides CPU-tuned unroll factors based on loop type and element size.

**Template parameters**
- `T` - loop type (Sum, DotProduct, Search, Copy, Transform)
- `ElementBytes` - size of element in bytes

**Default profile values**

| LoopType | 1B | 2B | 4B | 8B |
|----------|----|----|----|----|
| Sum | 16 | 8 | 4 | 8 |
| DotProduct | - | - | 8 | 8 |
| Search | 8 | 4 | 4 | 4 |
| Copy | 16 | 8 | 4 | 4 |
| Transform | 8 | 4 | 4 | 4 |

**Example**
```cpp
constexpr auto N = ilp::optimal_N<ilp::LoopType::Sum, sizeof(double)>;
// N = 8 for default profile
```

---

## LoopCtrl

```cpp
template<typename R = void>
struct LoopCtrl {
    bool ok = true;
    std::optional<R> return_value;  // only if R != void

    void break_loop();
    void return_with(R val);        // only if R != void
};
```

Control object passed to loop body for flow control.

**Member functions**
- `break_loop()` - exit loop immediately
- `return_with(val)` - exit loop and return value (requires `R != void`)

---

## Requirements

- C++23
- Header-only

## See also

- `std::for_each` - applies function to range
- `std::accumulate` - sums range
- `std::find_if` - finds element matching predicate
