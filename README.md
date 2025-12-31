# ILP_FOR

[![CI](https://github.com/mattyv/ilp_for/actions/workflows/ci.yml/badge.svg)](https://github.com/mattyv/ilp_for/actions/workflows/ci.yml)

Compile-time loop unrolling for early exit loops (`break`, `continue`, `return`). Avoids per-iteration bounds checks that `#pragma unroll` typically generates, enabling better instruction-level parallelism.
[What is ILP?](docs/ILP.md)

```cpp
#include <ilp_for.hpp>
```

---

## How It Works

Lets say you want to write the below code:

```cpp
int sum = 0;
for (size_t i = 0; i < n; ++i) {
    if (data[i] < 0) break;       // Early exit
    if (data[i] == 0) continue;   // Skip zeros
    sum += data[i];
}
```

Compilers *can* unroll this with `#pragma unroll` and will do a better job if dependency chains are broken down like so...
```cpp
constexpr size_t N = 4;
int sums[N] = {0};
#pragma unroll(4)
for (size_t i = 0; i < n; ++i) {
    if (data[i] < 0) break;       // Early exit
    if (data[i] == 0) continue;   // Skip zeros
    sums[i & (N-1)] += data[i];
}
int sum = (sums[0] + sums[1]) + (sums[2] + sums[3]);
```
But they do a bit of a crappy job and insert bounds checks after **each element** because [SCEV](https://llvm.org/devmtg/2018-04/slides/Absar-ScalarEvolution.pdf) cannot determine the trip count for loops with `break` so you end up with something like:

```
loop:
  if (i >= n) goto done;        // bounds check
  if (data[i] < 0) goto done;
  if (data[i] != 0) sums[i & 3] += data[i];
  i++;

  if (i >= n) goto done;        // bounds check (again!)
  if (data[i] < 0) goto done;
  if (data[i] != 0) sums[i & 3] += data[i];
  i++;

  if (i >= n) goto done;        // bounds check (again!)
  if (data[i] < 0) goto done;
  if (data[i] != 0) sums[i & 3] += data[i];
  i++;

  if (i >= n) goto done;        // bounds check (again!)
  if (data[i] < 0) goto done;
  if (data[i] != 0) sums[i & 3] += data[i];
  i++;

  goto loop;
done:
  sum = (sums[0] + sums[1]) + (sums[2] + sums[3]);
```

So what can you do? Create a main loop + remainder pattern that checks bounds only once per block? The compiler will give you nice machine code without the extra bounds checking, but this is messy and error prone and looks ghastly:

```cpp
constexpr size_t N = 4;
int sums[N] = {0};
size_t i = 0;
for (; i + 4 <= n; i += 4) {      // Main loop: bounds check once per 4 elements
    if (data[i+0] < 0) break;
    if (data[i+0] != 0) sums[0] += data[i+0];
    if (data[i+1] < 0) break;
    if (data[i+1] != 0) sums[1] += data[i+1];
    if (data[i+2] < 0) break;
    if (data[i+2] != 0) sums[2] += data[i+2];
    if (data[i+3] < 0) break;
    if (data[i+3] != 0) sums[3] += data[i+3];
}
for (; i < n; ++i) {              // Remainder
    if (data[i] < 0) break;
    if (data[i] == 0) continue;
    sums[i & (N-1)] += data[i];
}
int sum = (sums[0] + sums[1]) + (sums[2] + sums[3]);
```

See [why not pragma unroll?](docs/PRAGMA_UNROLL.md) for assembly evidence (~1.5x speedup).

But using ILP_FOR all you write is the below, which expands to effectively the same code as above. And despite a bit of macro CAPITALISATION doesn't look too bad:
```cpp
constexpr size_t N = 4;
int sums[N] = {0};
ILP_FOR(auto i, 0uz, n, N) {
    if (data[i] < 0) ILP_BREAK;
    if (data[i] == 0) ILP_CONTINUE;
    sums[i & (N-1)] += data[i];
} ILP_END;
int sum = (sums[0] + sums[1]) + (sums[2] + sums[3]);
```

I also decided to add `ILP_FOR_AUTO` variations which simplify the selection of the unroll factor for your hardware to help take the guesswork out and make portability between architectures more manageable (see below) (also probably saving you a few cycles if you want to make sure you're tuning to your hardware properly).

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

...or if you want something more portable, use `ILP_FOR_AUTO` with a [LoopType](#looptype-reference):

```cpp
ILP_FOR_AUTO(auto i, 0, n, Search, int) {
    if (data[i] < 0) ILP_BREAK;
    if (data[i] == 0) ILP_CONTINUE;
    process(data[i]);
} ILP_END;
```

Use the clang-tidy tool to check it suggested loop or unroll factor: see [tools/clang-tidy/](tools/clang-tidy/README.md)

```cpp
ILP_FOR_AUTO(auto i, 0, n, Add, int) { //incorrect LoopType
    if (data[i] < 0) ILP_BREAK;
    if (data[i] == 0) ILP_CONTINUE;
    process(data[i]);
} ILP_END;
```

```bash
ILP_FOR_AUTO(auto i, 0, n, Add, int) {
^~~~~~~~~~~~~~~~~~~~~
ILP_FOR_AUTO(auto i, 0, n, Search, int)
file.cpp:42:5: note: Portable fix: use ILP_FOR_AUTO with LoopType::Search
file.cpp:42:5: note: Architecture-specific fix for skylake: use ILP_FOR with N=4

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
    ILP_FOR_AUTO(auto i, 0, static_cast<int>(data.size()), Search, int) {
        if (data[i] == target) ILP_RETURN(i);
    } ILP_END_RETURN;
    return -1;
}
```

### Large Return Types

To save you typing the return type each time, `ILP_FOR` & `ILP_FOR_AUTO` store return values in a small buffer (SBO). The buffer size matches `sizeof(std::intmax_t)` on your target architecture (typically 8 bytes on 64-bit platforms). This works for types that are:
- **≤ SBO size** in size (typically 8 bytes)
- **≤ SBO size** alignment
- **Trivially destructible** (no custom destructor)

This covers `int`, `size_t`, pointers, and simple structs. Violations are caught at compile time via `static_assert`, so there's no risk of undefined behavior from type misuse. The implementation uses placement new and `std::launder` for well-defined object access.

For types that don't meet these requirements, just use `ILP_FOR_T` where you specify the return type explicitly. Though I would imagine that for most performant loops the average use case for ILP is going to operate with integral types.

To override the SBO size, define `ILP_SBO_SIZE` before including the header:
```bash
clang++ -std=c++20 -DILP_SBO_SIZE=16 mycode.cpp  # 16-byte SBO
``` 

```cpp
struct Result { int x, y, z; double value; };  // > 8 bytes

Result find_result(const std::vector<int>& data, int target) {
    ILP_FOR_T(Result, int i, 0, static_cast<int>(data.size()), 4) {
        if (data[i] == target) ILP_RETURN(Result{i, i*2, i*3, i*1.5});
    } ILP_END_RETURN;
    return Result{-1, 0, 0, 0.0};
}
```

---

## API Reference

### Loop Macros

| Macro | Description |
|-------|-------------|
| `ILP_FOR(var, start, end, N)` | Index loop with explicit N |
| `ILP_FOR_RANGE(var, range, N)` | Range-based loop with explicit N |
| `ILP_FOR_AUTO(var, start, end, LoopType, element_type)` | Index loop with auto-selected N |
| `ILP_FOR_RANGE_AUTO(var, range, LoopType, element_type)` | Range loop with auto-selected N |
| `ILP_FOR_T(type, var, start, end, N)` | Index loop for large return types (> 8 bytes) |
| `ILP_FOR_RANGE_T(type, var, range, N)` | Range loop for large return types |
| `ILP_FOR_T_AUTO(type, var, start, end, LoopType, element_type)` | Index loop for large types with auto-selected N |
| `ILP_FOR_RANGE_T_AUTO(type, var, range, LoopType, element_type)` | Range loop for large types with auto-selected N |

See [LoopType Reference](#looptype-reference) for available types (`Sum`, `Search`, `MinMax`, etc.)

Always end with `ILP_END`. If using `ILP_RETURN`, use `ILP_END_RETURN` instead.

### Control Flow

| Macro | Use In | Description |
|-------|--------|-------------|
| `ILP_CONTINUE` | Any loop | Skip to next iteration |
| `ILP_BREAK` | Loops | Exit loop |
| `ILP_RETURN(val)` | Loops with return type | Return `val` from enclosing function |

---

## Important Notes

### Use `auto&&` for Range Loops

When using `ILP_FOR_RANGE`, make sure you use `auto&&` to avoid copying each element (unless thats your thing):

```cpp
// Good - uses forwarding reference (zero copies)
ILP_FOR_RANGE(auto&& val, strings, 4) {
    process(val);
} ILP_END;

// Bad - copies each element into 'val' (slow for large types!)
ILP_FOR_RANGE(auto val, strings, 4) {
    process(val);
} ILP_END;
```

...this matters because range loops iterate over container elements directly. Using `auto` creates a copy of each element, while `auto&&` binds to the element in-place. You probably didn't need me to tell you this... but for completeness :). For a `std::vector<std::string>`, using `auto` would copy every string!

For index-based loops, just use `auto` since indices are just integers:

```cpp
ILP_FOR(auto i, 0, n, 4) {
    process(data[i]);
} ILP_END;
```

---

## When to Use ILP

**Use ILP_FOR for loops with early exit** (`break`, `continue`, `return`). Compilers can unroll these loops with `#pragma unroll`, but they insert per-iteration bounds checks that negate the performance benefit. ILP_FOR avoids this overhead (~1.5x speedup).

**Skip ILP for simple loops without early exit.** Compilers *can (amost most of the time)* produce optimal SIMD code automatically - all approaches (simple, pragma, ILP) *can* potentially compile to the same assembly. (It *may* not hurt to use ILP if you have no early exits so don't sweat it too much. In most of my tests it produced the same assembly)

```cpp
// Use ILP_FOR - early exit benefits from fewer bounds checks
ILP_FOR_AUTO(auto i, 0, n, Search, int) {
    if (data[i] == target) ILP_BREAK;
} ILP_END;

// Skip ILP - compiler auto-vectorizes loops without break
int sum = std::accumulate(data.begin(), data.end(), 0);
```

See [docs/PERFORMANCE.md](docs/PERFORMANCE.md) for benchmarks and [docs/PRAGMA_UNROLL.md](docs/PRAGMA_UNROLL.md) for why pragma doesn't help.

---

## Advanced

### CPU Architecture, Portability and _AUTO functions

You can target specific CPU architectures for optimal unroll factors. You really should do this if you plan to be portable or highly optimised.
If you don't specify anything the _AUTO function will expand to the *default* unroll values.

```bash
clang++ -std=c++20 -DILP_CPU_SKYLAKE      # Intel Skylake
clang++ -std=c++20 -DILP_CPU_ALDERLAKE    # Intel Alder Lake
clang++ -std=c++20 -DILP_CPU_APPLE_M1     # Apple M1
clang++ -std=c++20 -DILP_CPU_ZEN5         # AMD Zen 4/5
```

I source the locations where I have gathered data on each architecture so I believe this to be accurate.
If you do add a new architecture please let me know and I'll get it added.

### Debugging

If you need to debug your loop logic, you can disable ILP entirely:

```bash
clang++ -std=C++20 -DILP_MODE_SIMPLE -O0 -g mycode.cpp
```

This turns the macros into simple `for` loops with the same semantics:

| ILP Macro | Simple Mode Expansion |
|-----------|----------------------|
| `ILP_FOR(auto i, 0, n, 4)` | `for (auto i : ilp::iota(0, n))` |
| `ILP_FOR_AUTO(auto i, 0, n, Sum, int)` | `for (auto i : ilp::iota(0, n))` |
| `ILP_CONTINUE` | `continue` |
| `ILP_BREAK` | `break` |
| `ILP_RETURN(x)` | `return x` |
| `ILP_END` / `ILP_END_RETURN` | *(empty)* |

### LoopType Reference

When using `_AUTO` variants, you **must** to specify a 'LoopType' to auto-select the optimal unroll factor:

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

### Selecting LoopType Guide

**The basic principle:** Pick the LoopType for your loop's **bottleneck operation** - AKA the slowest or most congested one.

**Why?** The optimal unroll factor N follows `N ≈ Latency × Throughput` to keep enough operations in flight to saturate the execution unit and hide latency.

**Have mixed operations??? (e.g., adds AND multiplies):**

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

### Super Secret Tooling

If all else you can just use the `ilp-loop-analysis` clang-tidy check can detect patterns and suggest the correct LoopType automatically. Its pretty Beta but give it a go. See [tools/clang-tidy/](tools/clang-tidy/README.md).

### Reading CPU Profiles

The CPU profile headers are in `cpu_profiles/` and contain instruction timing data used to compute optimal N values. Each profile includes a reference table:

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

If you want to query the optimal unroll factor directly use...

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

---

## Formal Specifications with Axiom

I wanted to experiment with an idea: what if library maintainers could provide a formal knowledge RAG that LLMs can use to understand and write cleaner code using their library? The goal is to reduce hallucinations and improve the quality of LLM-generated code by giving them machine-readable contracts instead of relying on documentation or code alone, which may have gaps, ambiguities or cause the LLM to draw conclusion which are false. The knowledge is a dag/tree of knowledge grounded all the way down to the C/C++ standard.

### How it works

The [`knowledge/ilp_for_axioms.toml`](knowledge/ilp_for_axioms.toml) file contains 1000+ formal axioms auto extracted from the codebase covering:
- Macro preconditions (e.g., "N must be a compile-time constant expression")
- Type constraints (e.g., "loop variable must be integral")
- Runtime invariants (e.g., "start <= end for valid loop range")
- Template SFINAE conditions and concept requirements
- Violation behavior (compile error, runtime error, undefined behavior)

When you ask Claude Code or other AI tools about `ilp_for`, they can query these formal specifications to generate correct code and explain why certain patterns fail. Think of it as giving the LLM a precise understanding of the library's contracts instead of hoping it remembers or learns the details correctly.

The axiom system is built on [Axiom](https://github.com/mattyv/axiom) (included as a submodule at [`external/axiom/`](external/axiom/)), which provides:
- **Automated extraction** - parses your C++20 library source to extract preconditions, postconditions, invariants
- **MCP server integration** - query axioms from any AI tool that supports the Model Context Protocol
- **Formal verification** - enable static analysis and contract checking

See the [Axiom repository](https://github.com/mattyv/axiom) for documentation on extraction, verification, and LLM integration.

**[View Axiom Test Report](docs/axiom-test-report.md)** - Sample validations showing how Axiom catches incorrect ILP_FOR usage.

---

## Requirements

- C++20
- Header-only

---

## License

[Boost Software License 1.0](LICENSE)
