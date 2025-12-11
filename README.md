# ILP_FOR

[![CI](https://github.com/mattyv/ilp_for/actions/workflows/ci.yml/badge.svg)](https://github.com/mattyv/ilp_for/actions/workflows/ci.yml)

Compile-time loop unrolling for complex or early exit loops (i.e. for loops containing `break`, `continue`, `return`). Generates unrolled code with parallel condition evaluation, enabling better instruction-level parallelism than `#pragma unroll`.
[What is ILP?](docs/ILP.md)

```cpp
#include <ilp_for.hpp>
```

---

## How It Works

The library unrolls your loop body N times, allowing the CPU to execute multiple iterations in parallel:

Imagine you want to write:
```cpp
int sum = 0;
for (size_t i = 0; i < n; ++i)
{
    if (data[i] < 0) break;       // Error: stop
    if (data[i] == 0) continue;   // Skip zeros
    sum += data[i];               // Accumulate positives
}
```

While compilers *can* unroll this with `#pragma unroll`, they generate sequential condition checks ([why not just pragma unroll?](docs/PRAGMA_UNROLL.md)). For better ILP, you want parallel evaluation of conditions before sequential checking:
```cpp
int sum = 0;
size_t i = 0;
for (; i + 4 <= n; i += 4) {
    bool brk0 = data[i+0] < 0;    // Parallel evaluation
    bool brk1 = data[i+1] < 0;
    bool brk2 = data[i+2] < 0;
    bool brk3 = data[i+3] < 0;
    bool skp0 = data[i+0] == 0;
    bool skp1 = data[i+1] == 0;
    bool skp2 = data[i+2] == 0;
    bool skp3 = data[i+3] == 0;
    if (brk0) break;              // Sequential check
    if (!skp0) sum += data[i+0];
    if (brk1) break;
    if (!skp1) sum += data[i+1];
    if (brk2) break;
    if (!skp2) sum += data[i+2];
    if (brk3) break;
    if (!skp3) sum += data[i+3];
}
for (; i < n; ++i) {              // Remainder
    if (data[i] < 0) break;
    if (data[i] == 0) continue;
    sum += data[i];
}
```
...but this is tedious and error-prone!

This is where ILP_FOR macro helps. All you need to write is:
```cpp
int sum = 0;
ILP_FOR(auto i, 0uz, n, 4) {
    if (data[i] < 0) ILP_BREAK;
    if (data[i] == 0) ILP_CONTINUE;
    sum += data[i];
} ILP_END;
```
...which effectively generates the above unrolled code.

The library also has `ILP_FOR_AUTO` variations which simplify the selection of the unroll factor for your hardware, making portability more manageable (see below) and probably saving you a few cycles if you want to make sure you're tuning to your hardware properly.

---

## Contents

- [Quick Start](#quick-start)
- [Large Return Types](#large-return-types)
- [API Reference](#api-reference)
- [Important Notes](#important-notes)
- [When to Use ILP](#when-to-use-ilp)
- [Advanced](#advanced)
- [Test Coverage](#test-coverage)
- [Requirements](#requirements)

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

...or if you don't want to think about the unroll factor, use `ILP_FOR_AUTO` with a [LoopType](#looptype-reference):

```cpp
ILP_FOR_AUTO(auto i, 0, n, Search) {
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

...or with auto-selected unroll factor:

```cpp
int find_index(const std::vector<int>& data, int target) {
    ILP_FOR_AUTO(auto i, 0, static_cast<int>(data.size()), Search) {
        if (data[i] == target) ILP_RETURN(i);
    } ILP_END_RETURN;
    return -1;
}
```

### Large Return Types

To save you typing the return type each time `ILP_FOR` &  `ILP_FOR_AUTO` store return values in an 8-byte buffer, which covers you for `int`, `size_t`, and pointers. But for larger types, you'll need to use `ILP_FOR_T` to specify the return type:

```cpp
struct Result { int x, y, z; double value; };  // > 8 bytes

Result find_result(const std::vector<int>& data, int target) {
    ILP_FOR_T(Result, int i, 0, static_cast<int>(data.size()), 4) {
        if (data[i] == target) ILP_RETURN(Result{i, i*2, i*3, i*1.5});
    } ILP_END_RETURN;
    return Result{-1, 0, 0, 0.0};
}
```

### Function API 

After implementing the ILP_FOR api I figured there may be some value in implementing early return alternatives to some other std functions. So take from it what you will. 
Some std functions unroll very badly or cannot easily accomodate early return easily and still retain any unrolling.
The library provides `ilp::find` and `ilp::reduce`:

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
| `ILP_FOR_AUTO(var, start, end, LoopType)` | Index loop with auto-selected N |
| `ILP_FOR_RANGE_AUTO(var, range, LoopType)` | Range loop with auto-selected N |
| `ILP_FOR_T(type, var, start, end, N)` | Index loop for large return types (> 8 bytes) |
| `ILP_FOR_RANGE_T(type, var, range, N)` | Range loop for large return types |
| `ILP_FOR_T_AUTO(type, var, start, end, LoopType)` | Index loop for large types with auto-selected N |
| `ILP_FOR_RANGE_T_AUTO(type, var, range, LoopType)` | Range loop for large types with auto-selected N |

See [LoopType Reference](#looptype-reference) for available types (`Sum`, `Search`, `MinMax`, etc.)

End with `ILP_END`, or `ILP_END_RETURN` when using `ILP_RETURN`.

### Control Flow

| Macro/Function | Use In | Description |
|----------------|--------|-------------|
| `ILP_CONTINUE` | Any loop | Skip to next iteration |
| `ILP_BREAK` | Loops | Exit loop |
| `ILP_RETURN(val)` | Loops with return type | Return `val` from enclosing function |
| `-> std::optional<T>` | Reduce body | Return type for early exit support |
| `return std::nullopt` | Reduce body | Exit early from reduction |

### Function API 

| Function | Description |
|----------|-------------|
| `ilp::find<N>(start, end, body)` | Find first match, returns index |
| `ilp::find_range<N>(range, body)` | Find in range, returns iterator |
| `ilp::reduce<N>(start, end, init, op, body)` | Reduce with optional early exit |
| `ilp::reduce_range<N>(range, init, op, body)` | Range reduce |

Auto variants select optimal N based on [LoopType](#looptype-reference):
- `ilp::find_auto`, `ilp::find_range_auto` - defaults to `Search`
- `ilp::reduce_auto<LoopType>`, `ilp::reduce_range_auto<LoopType>` - requires LoopType (e.g., `Sum`, `MinMax`)

---

## Important Notes

### Use `auto&&` for Range Loops

When using range-based functions, you should use `auto&&` to avoid copying each element:

```cpp
// Good - uses forwarding reference (zero copies)
auto sum = ilp::reduce_range<4>(data, 0, std::plus<>{}, [&](auto&& val) {
    return val;
});

// Bad - copies each element into 'val' (slow for large types!)
auto sum = ilp::reduce_range<4>(data, 0, std::plus<>{}, [&](auto val) {
    return val;
});
```

...this matters because range loops iterate over container elements directly. Using `auto` creates a copy of each element, while `auto&&` binds to the element in-place. For a `std::vector<std::string>`, using `auto` would copy every string!

For index-based functions, just use `auto` since indices are just integers:

```cpp
ilp::reduce<4>(0, n, 0, std::plus<>{}, [&](auto i) { return data[i]; });
```

### Find Returns Index or Iterator

`ilp::find` returns the index directly (or the end value if not found):

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

If you need to exit a reduction early, use `std::optional` as your return type and return `std::nullopt` to stop:

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

### Associativity and Evaluation Order

For consistent results matching sequential evaluation, use associative operations: `+`, `*`, `min`, `max`, `&`, `|`, `^`

Non-associative operations will still execute, but results may differ due to parallel evaluation order. IEEE floating-point is technically non-associative due to rounding - parallel reduction may differ by a few ULPs from sequential. With `-ffast-math` you probably don't care. 

### Init Values

For custom operations, `init` must be the identity element for your operation:

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

**Use ILP_FOR for loops with early exit** (`break`, `return`). Compilers can unroll these loops with `#pragma unroll`, but they insert per-iteration bounds checks that negate the performance benefit. ILP_FOR avoids this overhead (~1.29x speedup).

**Use `ilp::reduce` for reductions with dependency chains** (min, max). Multiple independent accumulators break the dependency chain (~5.8x speedup).

**Skip ILP for simple loops without early exit.** Compilers produce optimal SIMD code automatically - all approaches (simple, pragma, ILP) compile to the same assembly.

```cpp
// Use ILP_FOR - early exit benefits from fewer bounds checks
ILP_FOR_AUTO(auto i, 0, n, Search) {
    if (data[i] == target) ILP_BREAK;
} ILP_END;

// Use ilp::reduce - breaks dependency chain for parallel min
int min_val = ilp::reduce_range_auto<ilp::LoopType::MinMax>(
    data, INT_MAX, [](int a, int b) { return std::min(a, b); }, [](auto&& v) { return v; });

// Skip ILP - compiler auto-vectorizes loops without break
int sum = std::accumulate(data.begin(), data.end(), 0);
```

See [docs/PERFORMANCE.md](docs/PERFORMANCE.md) for benchmarks and [docs/PRAGMA_UNROLL.md](docs/PRAGMA_UNROLL.md) for why pragma doesn't help.

---

## Advanced

### CPU Architecture

You can target specific CPU architectures for optimal unroll factors. This is highly advisable if you plan to be portable or highly optimised:

```bash
clang++ -std=c++23 -DILP_CPU=skylake    # Intel Skylake
clang++ -std=c++23 -DILP_CPU=apple_m1   # Apple M1
clang++ -std=c++23 -DILP_CPU=zen5       # AMD Zen 5
```

I source the locations where I have gathered data on the each architecture so I believe this to be accurate.
If you do add a new architecture please let me know and I'll get it added.

### Debugging

If you need to debug your loop logic, you can disable ILP entirely:

```bash
clang++ -std=c++23 -DILP_MODE_SIMPLE -O0 -g mycode.cpp
```

...all macros then expand to simple `for` loops with the same semantics.

### LoopType Reference

When using `_AUTO` variants, you need to specify a 'LoopType' to auto-select the optimal unroll factor:

| LoopType | Operation | Use Case |
|----------|-----------|----------|
| `Sum` | `acc += val` | Summation, accumulation |
| `DotProduct` | `acc += a * b` | Dot products, FMA |
| `Search` | Early exit | find, any_of, all_of |
| `Copy` | `dst = src` | Memory copy |
| `Transform` | `dst = f(src)` | Element-wise transforms |
| `Multiply` | `acc *= val` | Product reduction |
| `Divide` | `val / const` | Division |
| `Sqrt` | `sqrt(val)` | Square root |
| `MinMax` | `min/max(acc, val)` | Min/max reduction |
| `Bitwise` | `&`, `\|`, `^` | Bitwise AND/OR/XOR |
| `Shift` | `<<`, `>>` | Bit shifting |

### Selecting LoopType

**The principle:** Pick the LoopType for your loop's **bottleneck operation** - the slowest or most congested one.

**Why?** The optimal unroll factor N follows `N ≈ Latency × Throughput` to keep enough operations in flight to saturate the execution unit and hide latency.

**For mixed operations (e.g., adds AND multiplies):**

1. **Identify the critical path** - operations with dependencies form a chain; independent operations can overlap
2. **Pick the slowest operation on that path:**
   - `acc += data[i] * weight[i]` → This is FMA, use `DotProduct`
   - `acc += data[i]; acc *= factor;` → Multiply is slower, use `Multiply`
   - Mostly adds with occasional multiply → `Sum`
   - Mostly multiplies with occasional add → `Multiply`

3. **Early exit dominates everything:**
   - If your loop has `ILP_BREAK` or `ILP_RETURN`, branch prediction is usually the bottleneck
   - Use `Search` regardless of what computation happens inside

**Quick decision tree:**
```
Has early exit (break/return)?     → Search
Doing acc += a * b (FMA)?          → DotProduct
Doing acc += val?                  → Sum
Doing acc *= val?                  → Multiply
Doing min/max?                     → MinMax
Doing bitwise ops?                 → Bitwise
Unsure?                            → Search (safe default)
```

### Reading CPU Profiles

The CPU profile headers in `cpu_profiles/` contain instruction timing data used to compute optimal N values. Each profile includes a reference table:

```
| Instruction    | Use Case | Latency | RThr | L×TPC |
| VFMADD231PS/PD | FMA      |    4    | 0.50 |   8   |
| VADDPS/VADDPD  | FP Add   |    4    | 0.50 |   8   |
| VPMULLD        | Int Mul  |   10    | 1.00 |  10   |
```

**Column definitions:**
- **Latency (L)**: Cycles from input ready to output ready
- **RThr**: Reciprocal throughput - cycles between starting new operations
- **TPC**: Throughput per cycle = 1/RThr
- **L×TPC**: The optimal unroll factor N

**The formula:** `optimal_N = Latency × TPC`

If FP add has L=4 and TPC=2, then N = 8 independent adds are needed to keep the pipeline saturated and hide the 4-cycle latency.

**Creating custom profiles:** Look up your CPU's instruction timings at [uops.info](https://uops.info) or [Agner Fog's tables](https://www.agner.org/optimize/instruction_tables.pdf), then create a header following the existing format in `cpu_profiles/`.

### optimal_N

If you want to query the optimal unroll factor directly:

```cpp
constexpr auto N = ilp::optimal_N<ilp::LoopType::Sum, double>;
```

Default Header values by type:

| LoopType | int32 | int64 | float | double |
|----------|-------|-------|-------|--------|
| Sum | 4 | 4 | 8 | 8 |
| DotProduct | - | - | 8 | 8 |
| Search | 4 | 4 | 4 | 4 |
| MinMax | 4 | 4 | 8 | 8 |
| Multiply | 8 | 8 | 8 | 8 |
| Bitwise | 8 | 8 | - | - |
| Shift | 8 | 8 | - | - |
| Copy | 4 | 4 | 4 | 4 |
| Transform | 4 | 4 | 4 | 4 |
| Divide | - | - | 8 | 8 |
| Sqrt | - | - | 8 | 8 |

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
