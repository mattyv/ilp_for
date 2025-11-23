# ILP_FOR

Compile-time loop unrolling with multi-accumulator ILP support.

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

// ----- Index-based loops -----

template<std::size_t N, std::integral T, typename F>
void for_loop_simple(T start, T end, F&& body);

template<std::size_t N, std::integral T, typename F>
void for_loop(T start, T end, F&& body);

template<typename R, std::size_t N, std::integral T, typename F>
std::optional<R> for_loop_ret(T start, T end, F&& body);

template<typename R, std::size_t N, std::integral T, typename F>
std::optional<R> for_loop_ret_simple(T start, T end, F&& body);

// ----- Step loops -----

template<std::size_t N, std::integral T, typename F>
void for_loop_step_simple(T start, T end, T step, F&& body);

template<std::size_t N, std::integral T, typename F>
void for_loop_step(T start, T end, T step, F&& body);

template<typename R, std::size_t N, std::integral T, typename F>
std::optional<R> for_loop_step_ret(T start, T end, T step, F&& body);

// ----- Range-based loops -----

template<std::size_t N, std::ranges::random_access_range Range, typename F>
void for_loop_range_simple(Range&& range, F&& body);

template<std::size_t N, std::ranges::random_access_range Range, typename F>
void for_loop_range(Range&& range, F&& body);

template<typename R, std::size_t N, std::ranges::random_access_range Range, typename F>
std::optional<R> for_loop_range_ret_simple(Range&& range, F&& body);

template<typename R, std::size_t N, std::ranges::random_access_range Range, typename F>
std::optional<R> for_loop_range_idx_ret_simple(Range&& range, F&& body);

// ----- Reduce (multi-accumulator) -----

template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
auto reduce(T start, T end, Init init, BinaryOp op, F&& body);

template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
auto reduce_simple(T start, T end, Init init, BinaryOp op, F&& body);

template<std::size_t N, std::integral T, typename F>
auto reduce_sum(T start, T end, F&& body);

template<std::size_t N, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
auto reduce_range(Range&& range, Init init, BinaryOp op, F&& body);

template<std::size_t N, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
auto reduce_range_simple(Range&& range, Init init, BinaryOp op, F&& body);

template<std::size_t N, std::ranges::random_access_range Range, typename F>
auto reduce_range_sum(Range&& range, F&& body);

template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
auto reduce_step_simple(T start, T end, T step, Init init, BinaryOp op, F&& body);

template<std::size_t N, std::integral T, typename F>
auto reduce_step_sum(T start, T end, T step, F&& body);

} // namespace ilp
```

---

## Benchmark Results

Performance comparison with 10M elements:

| Operation | std | ILP | Speedup |
|-----------|-----|-----|---------|
| Min | 2.9G/s | 16.0G/s | **5.5x** |
| All-of | 2.0G/s | 13.2G/s | **6.8x** |
| Count | 7.5G/s | 12.5G/s | **1.7x** |
| Sum with break | 2.8G/s | 4.5G/s | **1.6x** |
| Simple sum | 18.1G/s | 16.7G/s | 0.9x |

**Use ILP for:** min/max, product, count, all-of, loops with early exit

**Skip ILP for:** simple sums (compiler auto-vectorizes better)

---

## Macros - Preferred Interface

> **Note:** In all macros, the `loop_var_name` parameter (e.g., `i`) is **defined by the macro** - you do not need to declare it beforehand. The type is deduced from `start` (e.g., `0` → `int`, `0uz` → `size_t`). Similarly, `val` in range-based macros is defined by the macro with type deduced from the range's value type.

### Loop Macros

#### ILP_FOR_SIMPLE

```cpp
#define ILP_FOR_SIMPLE(loop_var_name, start, end, N) /* ... */
```

Simple loop macro without control flow.

**Example**
```cpp
std::vector<int> data(100);
// Transform in place (no dependency chain)
ILP_FOR_SIMPLE(i, 0, (int)data.size(), 4) {
    data[i] = data[i] * 2 + 1;
} ILP_END;

// For sums, use ILP_REDUCE_* instead
```

---

#### ILP_FOR

```cpp
#define ILP_FOR(loop_var_name, start, end, N) /* ... */
```

Loop macro with break/continue support.

**Example**
```cpp
std::vector<int> data(100);
ILP_FOR(i, 0, (int)data.size(), 4) {
    if (data[i] < 0) ILP_BREAK;
    if (data[i] == 0) ILP_CONTINUE;
    process(data[i]);
} ILP_END;
```

---

#### ILP_FOR_RET

```cpp
#define ILP_FOR_RET(ret_type, loop_var_name, start, end, N) /* ... */
```

Loop macro with return value support.

**Example**
```cpp
std::vector<int> data(100);
int target = 42;
ILP_FOR_RET(int, i, 0, (int)data.size(), 4) {
    if (data[i] == target) {
        ILP_RETURN(i);
    }
} ILP_END_RET;
// Returns from enclosing function if found
```

---

#### ILP_FOR_RANGE_SIMPLE

```cpp
#define ILP_FOR_RANGE_SIMPLE(var, range, N) /* ... */
```

Range-based loop without control flow.

**Example**
```cpp
std::vector<int> v = {1, 2, 3, 4, 5};
ILP_FOR_RANGE_SIMPLE(val, v, 4) {
    process(val);  // independent operations
} ILP_END;
// For sums, use ILP_REDUCE_RANGE_SUM instead
```

---

#### ILP_FOR_STEP_SIMPLE

```cpp
#define ILP_FOR_STEP_SIMPLE(loop_var_name, start, end, step, N) /* ... */
```

Step loop without control flow.

**Example**
```cpp
std::vector<int> data(100);
ILP_FOR_STEP_SIMPLE(i, 0, (int)data.size(), 2, 4) {
    data[i] = data[i] * 2;  // every 2nd element
} ILP_END;
// For sums, use ILP_REDUCE_STEP_SUM instead
```

---

### Reduce Macros

#### ILP_REDUCE_RANGE_SUM

```cpp
#define ILP_REDUCE_RANGE_SUM(var, range, N) /* ... */
```

Multi-accumulator sum over a range.

**Example**
```cpp
std::vector<int> data = {1, 2, 3, 4, 5};
int sum = ILP_REDUCE_RANGE_SUM(val, data, 4) {
    return val;
} ILP_END_REDUCE;
```

---

#### ILP_REDUCE_RANGE_SIMPLE

```cpp
#define ILP_REDUCE_RANGE_SIMPLE(op, init, var, range, N) /* ... */
```

Multi-accumulator reduce with custom operation.

**Example**
```cpp
// Find minimum (5.5x faster than std::min_element)
int min_val = ILP_REDUCE_RANGE_SIMPLE(
    [](int a, int b) { return std::min(a, b); },
    INT_MAX, val, data, 4
) {
    return val;
} ILP_END_REDUCE;

// Count matching elements
int count = ILP_REDUCE_RANGE_SIMPLE(std::plus<>{}, 0, val, data, 4) {
    return (val == target) ? 1 : 0;
} ILP_END_REDUCE;
```

---

#### ILP_REDUCE

```cpp
#define ILP_REDUCE(op, init, loop_var_name, start, end, N) /* ... */
```

Index-based reduce with break support.

**Example**
```cpp
std::vector<int> data(1000);
int stop_at = 500;
int sum = ILP_REDUCE(std::plus<>{}, 0, i, 0, (int)data.size(), 4) {
    if (i >= stop_at) {
        ILP_BREAK_RET(0);  // break and return neutral element
    }
    return data[i];
} ILP_END_REDUCE;
```

---

#### ILP_REDUCE_SUM

```cpp
#define ILP_REDUCE_SUM(loop_var_name, start, end, N) /* ... */
```

Index-based sum.

**Example**
```cpp
std::vector<int> data(1000);
int sum = ILP_REDUCE_SUM(i, 0, (int)data.size(), 4) {
    return data[i];
} ILP_END_REDUCE;
```

---

#### ILP_REDUCE_STEP_SUM

```cpp
#define ILP_REDUCE_STEP_SUM(loop_var_name, start, end, step, N) /* ... */
```

Step-based sum.

**Example**
```cpp
std::vector<int> data(1000);
int sum = ILP_REDUCE_STEP_SUM(i, 0, (int)data.size(), 2, 4) {
    return data[i];  // every 2nd element
} ILP_END_REDUCE;
```

---

### Control Flow Macros

| Macro | Use In | Description |
|-------|--------|-------------|
| `ILP_CONTINUE` | Any loop | Skip to next iteration |
| `ILP_BREAK` | Loops | Exit loop (returns void) |
| `ILP_BREAK_RET(val)` | Reduce | Exit and return value |
| `ILP_RETURN(val)` | `*_RET` loops | Exit loop and return from enclosing function |

---

### Loop Endings

| Macro | Use With |
|-------|----------|
| `ILP_END` | All loops without return |
| `ILP_END_RET` | `ILP_FOR_RET`, `ILP_FOR_RET_SIMPLE` |
| `ILP_END_REDUCE` | All reduce macros |

---

## Member Functions

### reduce_simple

```cpp
template<std::size_t N = 4, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_simple(T start, T end, Init init, BinaryOp op, F&& body);
```

Multi-accumulator reduce without break support.

**Parameters**
- `start` - beginning of range (inclusive)
- `end` - end of range (exclusive)
- `init` - initial value for each accumulator
- `op` - binary operation to combine results
- `body` - callable with signature `auto(T)` returning contribution

**Return value**

Result of reducing all contributions with `op`.

**Example**
```cpp
std::vector<int> data(1000);
// Find minimum
int min_val = ilp::reduce_simple<4>(0, (int)data.size(), INT_MAX,
    [](int a, int b) { return std::min(a, b); },
    [&](int i) { return data[i]; });
```

---

### reduce

```cpp
template<std::size_t N = 4, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
auto reduce(T start, T end, Init init, BinaryOp op, F&& body);
```

Multi-accumulator reduce with break support.

**Parameters**
- `start` - beginning of range
- `end` - end of range
- `init` - initial value for each accumulator
- `op` - binary operation
- `body` - callable with signature `auto(T, LoopCtrl<void>&)`

**Example**
```cpp
std::vector<int> data(1000);
int stop_at = 500;
int sum = ilp::reduce<4>(0, (int)data.size(), 0, std::plus<>{},
    [&](int i, auto& ctrl) {
        if (i >= stop_at) {
            ctrl.break_loop();
            return 0;
        }
        return data[i];
    });
```

---

### reduce_range_sum

```cpp
template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_sum(Range&& range, F&& body);
```

Multi-accumulator sum over a range.

**Parameters**
- `range` - random access range
- `body` - callable returning contribution for each element

**Example**
```cpp
std::vector<int> v = {1, 2, 3, 4, 5};
int sum = ilp::reduce_range_sum<4>(v, [](int x) { return x; });
```

---

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
std::vector<int> data(100);
// Transform in place
ilp::for_loop_simple<4>(0, (int)data.size(), [&](int i) {
    data[i] = data[i] * 2 + 1;
});
// For sums, use reduce_* functions instead
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
std::vector<int> data(100);
ilp::for_loop<4>(0, (int)data.size(), [&](int i, auto& ctrl) {
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
std::vector<int> data(100);
int target = 42;
auto result = ilp::for_loop_ret<int, 4>(0, (int)data.size(), [&](int i, auto& ctrl) {
    if (data[i] == target) {
        ctrl.return_with(i);
    }
});
if (result) std::cout << "Found at " << *result;
```

---

### for_loop_range_idx_ret_simple

```cpp
template<typename R, std::size_t N = 4, std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, std::size_t>
std::optional<R> for_loop_range_idx_ret_simple(Range&& range, F&& body);
```

Range-based loop with index and early return.

**Parameters**
- `range` - random access range
- `body` - callable with signature `std::optional<R>(element, index)`

**Example**
```cpp
std::vector<int> data(100);
int target = 42;
auto idx = ilp::for_loop_range_idx_ret_simple<size_t, 4>(data,
    [&](auto val, auto i) -> std::optional<size_t> {
        if (val == target) return i;
        return std::nullopt;
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

- `std::accumulate` - sequential reduction
- `std::min_element` - sequential min (ILP is 5.5x faster)
- `std::count_if` - sequential count (ILP is 1.7x faster)
