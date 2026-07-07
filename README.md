# ILP_FOR

[![CI](https://github.com/mattyv/ilp_for/actions/workflows/ci.yml/badge.svg)](https://github.com/mattyv/ilp_for/actions/workflows/ci.yml)

Compile-time loop unrolling for early-exit loops (`break`, `continue`, `return`). Unrolls without the per-iteration bounds checks `#pragma unroll` generates, so the CPU can actually exploit the instruction-level parallelism the unroll was supposed to buy.
[What is ILP?](docs/ILP.md)

```cpp
#include <ilp_for.hpp>
```

Headline numbers (Apple M2, Clang 19, 10M elements, `-O3 -march=native`):

| Loop Type | Simple | Pragma | ILP | Speedup |
|-----------|--------|--------|-----|---------|
| `ILP_FOR` with `ILP_BREAK` | 1.46ms | 1.46ms | 0.94ms | **1.56x** |
| `ILP_FOR` with `ILP_RETURN` | 1.68ms | 1.51ms | 0.94ms | **1.79x** |
| `ILP_FOR_RANGE` with `ILP_BREAK` | 2.21ms | - | 0.94ms | **2.35x** |

Full benchmarks in [docs/PERFORMANCE.md](docs/PERFORMANCE.md).

---

## How It Works

Start with a loop you'd write on any ordinary day:

```cpp
int sum = 0;
for (size_t i = 0; i < n; ++i) {
    if (data[i] < 0) break;       // Early exit
    if (data[i] == 0) continue;   // Skip zeros
    sum += data[i];
}
```

The early exit rules out vectorization, but there's still instruction-level parallelism on the table: split the accumulator into four independent chains, ask the compiler to unroll, and four additions can be in flight at once...
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
...except the compiler doesn't hold up its end. [SCEV](https://llvm.org/devmtg/2018-04/slides/Absar-ScalarEvolution.pdf) cannot compute a trip count for a loop that might `break`, so the unroller plays it safe and re-checks the bounds after **every element**. What you get is:

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

The way out is the classic main-loop-plus-remainder pattern: check the bounds once per block of four, then mop up the stragglers. The compiler rewards you with clean machine code. Your code reviewers will be less impressed:

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

See [why not pragma unroll?](docs/PRAGMA_UNROLL.md) for the assembly evidence (~1.5x speedup).

`ILP_FOR` generates that same block-plus-remainder structure from something you can actually read. A bit of macro CAPITALISATION aside, it's the loop you started with:
```cpp
constexpr size_t N = 4;
int sums[N] = {0};
ILP_FOR(auto i, size_t{0}, n, N) {
    if (data[i] < 0) ILP_BREAK;
    if (data[i] == 0) ILP_CONTINUE;
    sums[i & (N-1)] += data[i];
} ILP_END;
int sum = (sums[0] + sums[1]) + (sums[2] + sums[3]);
```

The `ILP_FOR_AUTO` variants go a step further and choose the unroll factor for you, from per-architecture instruction-timing tables — the same source stays properly tuned across targets instead of hard-coding one machine's sweet spot ([details below](#cpu-architecture-portability-and-_auto-functions)).

Everything the macros do is sugar over a plain function API — if your codebase bans macros, use that directly ([Macro-free API](#macro-free-api)).

---

## Contents

- [Quick Start](#quick-start)
- [Large Return Types](#large-return-types)
- [API Reference](#api-reference)
- [Macro-free API](#macro-free-api)
- [Important Notes](#important-notes)
- [When to Use ILP](#when-to-use-ilp)
- [Advanced](#advanced)
  - [clang-tidy LoopType analysis](#clang-tidy-looptype-analysis)
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

Not sure which LoopType fits? The bundled clang-tidy check will suggest one — and flag the one you guessed wrong (see [tools/clang-tidy/](tools/clang-tidy/README.md)):

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

So you never have to spell the return type at the loop site, `ILP_FOR` and `ILP_FOR_AUTO` carry returned values in a small inline buffer (SBO), sized to `sizeof(std::intmax_t)` on your target — typically 8 bytes on 64-bit platforms. That works for any type that is:
- **≤ SBO size** in size (typically 8 bytes)
- **≤ SBO size** alignment
- **Trivially destructible** (no custom destructor)

This covers `int`, `size_t`, pointers, and simple structs. Size/alignment/destructibility
violations are caught at compile time via `static_assert`. The implementation uses
placement new and `std::launder` for well-defined object access.

**One thing `static_assert` can't catch:** the untyped path is type-erased, so if the
value you `ILP_RETURN` and the type you recover it as (usually the enclosing
function's declared return type) are two *different* same-size-or-smaller types —
say, `ILP_RETURN(some_int)` inside a function that returns `long` — the bytes get
reinterpreted, not converted. That's silently wrong in a release build. Debug builds
(any build without `-DNDEBUG`) catch this automatically: on a mismatch, the program
aborts with a message naming both types, rather than returning a wrong value. This
also covers mismatches across [nested](#nested-loops) propagation, not just the
top-level case. Force it on in a release build with `-DILP_DEBUG_TYPECHECK`, or force
it off in a debug build with `-DILP_NO_DEBUG_TYPECHECK` — the latter is only useful if
you need debug-build binary layout to match release exactly, since the check adds one
pointer to the SBO when enabled; don't mix TUs built with and without it. When in
doubt, just make sure your `ILP_RETURN` argument's type matches what you're recovering
it as, or use `ILP_FOR_T` to make the type explicit and skip this whole class of bug.

Types that don't fit take `ILP_FOR_T`, which names the return type explicitly. In practice most hot loops traffic in integers, indices, and pointers, so the SBO covers the common case and `ILP_FOR_T` is the exception.

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

Always end with `ILP_END`. If using `ILP_RETURN`, use `ILP_END_RETURN` instead —
mixing them up is a **compile-time error** naming the fix, not a runtime bug: a body
that calls `ILP_RETURN` but is closed with plain `ILP_END` fails to build. The macro
layer is the same in both build modes, so this holds under `ILP_MODE_SIMPLE` too.

### Control Flow

| Macro | Use In | Description |
|-------|--------|-------------|
| `ILP_CONTINUE` | Any loop | Skip to next iteration |
| `ILP_BREAK` | Loops | Exit loop |
| `ILP_RETURN(val)` | Loops with return type | Return `val` from enclosing function |

### Optimization Hints

| Macro | Description |
|-------|-------------|
| `ILP_FLATTEN` | Function annotation — force-inline the loop's call tree so GCC fuses independent body predicates |

`ILP_FLATTEN` prefixes a **function** (not a loop), and applies equally to the macro
and the function API — the annotation goes on the enclosing function in both cases:

```cpp
// Macro API
ILP_FLATTEN size_t first_odd_over(const uint32_t* d, size_t n, uint32_t t) {
    ILP_FOR(auto i, size_t{0}, n, 4) {
        if (d[i] % 2 == 0) ILP_CONTINUE;
        if (d[i] > t)      ILP_RETURN(i);
    } ILP_END_RETURN;
    return n;
}

// Function API - same annotation, same place
ILP_FLATTEN size_t first_odd_over(const uint32_t* d, size_t n, uint32_t t) {
    auto r = ilp::for_loop<4>(size_t{0}, n, [&](auto i, auto& ctrl) {
        if (d[i] % 2 == 0) return;                  // continue
        if (d[i] > t)      return ctrl.return_with(i);
    });
    if (r) return *std::move(r);
    return n;
}
```

**When to use it:** only on GCC, only when a loop body has **two or more independent
predicates** and the least-predictable one is checked first (e.g. a 50/50 parity skip
before a rarely-true threshold check), and only if you actually measure a
branch-misprediction cliff (it appears once the data spills cache). It is not a
general "make it faster" knob — on a well-predicted loop it does nothing useful.

**Why it works:** GCC only fuses those predicates into one branch if the loop's call
tree is inlined by its *early* inliner; through both APIs several layers inline later
than that, so the fusion is missed and the coin-flip check stays per-element.
`[[gnu::flatten]]` forces the whole tree to inline early, restoring the fusion (~10-15x
on the affected loops; verified 26% → 0% branch-misses, GCC 14/15). Clang doesn't need
it. Two limits: it only takes effect under `NDEBUG` (or `-DILP_NO_DEBUG_TYPECHECK`),
and `[[gnu::flatten]]` force-inlines *everything* the function calls, so keep the
annotated function small. Reordering the predicates (selective condition first) fixes
the same cliff with no annotation. Full story: the GCC predicate-order caveat in
[docs/PRAGMA_UNROLL.md](docs/PRAGMA_UNROLL.md).

---

## Macro-free API

The macros are sugar over a plain function API (`ilp::for_loop` and friends in
`ilp_for/detail/loops_ilp.hpp`). If your style guide bans macros, call it directly —
same unrolling, same semantics, no `ILP_*` tokens:

```cpp
// Macro version
ILP_FOR(auto i, 0, n, 4) {
    if (data[i] < 0) ILP_BREAK;
    if (data[i] == 0) ILP_CONTINUE;
    if (data[i] == target) ILP_RETURN(i);
    sum += data[i];
} ILP_END_RETURN;

// Function API equivalent
auto r = ilp::for_loop<4>(0, n, [&](auto i, auto& ctrl) {
    if (data[i] < 0)       return ctrl.break_loop();  // ILP_BREAK
    if (data[i] == 0)      return;                    // ILP_CONTINUE
    if (data[i] == target) return ctrl.return_with(i); // ILP_RETURN(i)
    sum += data[i];
});
if (r) return *std::move(r);   // ILP_END_RETURN
```

A bare `return;` from the body lambda means *continue* — the natural, idiomatic
meaning for a lambda, and not a footgun the way it is inside the `ILP_FOR` macro
expansion (see [DESIGN_NOTES.md](docs/DESIGN_NOTES.md)).

### Break/continue-only loops: `ilp::for_each`

`for_loop`/`for_loop_range` return a `[[nodiscard]]` `ForResult`, even for loops
that never call `return_with` — the equivalent of `ILP_FOR ... ILP_END` (no
`ILP_RETURN`). For that case, prefer `ilp::for_each`/`ilp::for_each_range`, which
return `void`:

```cpp
// Macro version (no ILP_RETURN, so ILP_END)
ILP_FOR(auto i, 0, n, 4) {
    if (data[i] < 0) ILP_BREAK;
    sum += data[i];
} ILP_END;

// Function API equivalent - no [[maybe_unused]] auto r = ... needed
ilp::for_each<4>(0, n, [&](auto i, auto& ctrl) {
    if (data[i] < 0) return ctrl.break_loop();
    sum += data[i];
});
```

Calling `ctrl.return_with(x)` inside a `for_each` body is a compile error pointing
you at `ilp::for_loop` instead — `for_each` genuinely cannot return a value out of
the enclosing function, so there is nothing to discard and nothing to `nodiscard`.

### Equivalence table

| Macro | Function API |
|-------|--------------|
| `ILP_FOR(auto i, 0, n, 4) {...} ILP_END;` (no `ILP_RETURN`) | `ilp::for_each<4>(0, n, [&](auto i, auto& ctrl){...});` |
| `ILP_FOR(auto i, 0, n, 4) {...} ILP_END_RETURN;` | `ilp::for_loop<4>(0, n, [&](auto i, auto& ctrl){...});` |
| `ILP_FOR_AUTO(auto i, 0, n, Search, int) {...} ILP_END;` | `ilp::for_each<ilp::optimal_N<ilp::LoopType::Search, int>>(0, n, [&](auto i, auto& ctrl){...});` |
| `ILP_FOR_RANGE(auto&& v, r, 4) {...} ILP_END;` | `ilp::for_each_range<4>(r, [&](auto&& v, auto& ctrl){...});` |
| `ILP_FOR_T(Result, auto i, 0, n, 4) {...} ILP_END_RETURN;` | `ilp::for_loop_typed<Result, 4>(0, n, [&](auto i, auto& ctrl){...});` |
| `ILP_BREAK` | `return ctrl.break_loop();` |
| `ILP_CONTINUE` | `return;` |
| `ILP_RETURN(x)` | `return ctrl.return_with(x);` (only on `for_loop`/`for_loop_typed` ctrl - poisoned on `for_each`) |
| `ILP_END_RETURN` | `if (r) return *std::move(r);` |

### Per-loop debug mode (function-API exclusive)

`ILP_MODE_SIMPLE` is a translation-unit-wide define. The function API also accepts
an explicit `ilp::Mode` template argument, which overrides `ilp::default_mode` for
just that one loop — useful for stepping through a single hot loop without
de-ILPing the whole file:

```cpp
// Whole file built normally, but de-ILP just this loop while debugging it:
auto r = ilp::for_loop<4, ilp::Mode::Simple>(0, n, [&](auto i, auto& ctrl) { ... });
```

`ilp::Mode::Simple` runs only the tail/remainder loop — the same single
bounds-check-per-iteration code path `ILP_MODE_SIMPLE` produces for the macros
(both go through the same body-lambda mechanism, so there's no macro-vs-function-API
difference in debugger experience here).

---

## Important Notes

### Use `auto&&` for Range Loops

With `ILP_FOR_RANGE`, declare the loop variable `auto&&` so elements bind in place instead of being copied (unless copying is the point):

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

None of this is news if you've written a range-for before — but it bites harder in a loop you chose specifically for speed: over a `std::vector<std::string>`, `auto` copies every single string.

Range loops require a **random-access range** (`std::vector`, `std::array`, `std::span`, raw arrays...) — the unrolled blocks need indexed access. A `std::list`, `std::set`, or non-random-access view won't compile; those containers can't benefit from ILP anyway, so use an ordinary range-for there.

Index-based loops are immune — the loop variable is an integer, so plain `auto` is right:

```cpp
ILP_FOR(auto i, 0, n, 4) {
    process(data[i]);
} ILP_END;
```

### Nested Loops

`ILP_RETURN` returns from the enclosing C++ function at any nesting depth, in both
build modes (the macro layer is unconditional — see [Debugging](#debugging)):

```cpp
int find_first_match(const std::vector<std::vector<int>>& rows, int target) {
    ILP_FOR(auto r, std::size_t{0}, rows.size(), 2) {
        ILP_FOR(auto c, std::size_t{0}, rows[r].size(), 4) {
            if (rows[r][c] == target) ILP_RETURN(static_cast<int>(r * 100 + c));
        } ILP_END_RETURN;   // required: this loop carries the inner value outward
    } ILP_END_RETURN;
    return -1;
}
```

**Every enclosing loop on the path out must be closed with `ILP_END_RETURN`** —
each one carries the value one level further. Closing an enclosing loop with plain
`ILP_END` instead is a **compile-time error** naming the fix, since a break-only
loop has nowhere to put the value.

**Type caveat:** the propagated value's type must be the same at every level it
passes through. An untyped `ILP_FOR` (no `ILP_FOR_T`) recovers a nested value via
the same type-erased SBO recovery used at the top level (see
[Large Return Types](#large-return-types) and
[DESIGN_NOTES.md](docs/DESIGN_NOTES.md) item 3) — it reinterprets the stored bytes
as whatever type the *next* level outward expects, rather than converting. So
`ILP_RETURN(some_int)` propagating out through an untyped loop into an
`int`-returning function is fine; propagating that same `int` out through an
`ILP_FOR_T(long, ...)` outer loop is not — the bytes get reinterpreted as `long`.
Keep the propagated type consistent, or use `ILP_FOR_T` at every level that isn't
already returning the exact type you want. Debug builds catch this particular
mismatch automatically and abort naming both types — see the debug-mode type check
note in [Large Return Types](#large-return-types).

Two things this does *not* cover:
- **A loop macro nested inside a function-API `for_loop`/`for_each` body — do not do
  this, it is undefined behavior, not just a wrong answer.** The ctrl variable
  there has whatever name your lambda parameter used, so the macro's `ILP_RETURN`
  can't find it and treats itself as top-level. The `ILP_END_RETURN` it expands to
  ends up injecting a `return` directly into your outer lambda's body — on any outer
  iteration where the nested macro loop's search doesn't find a match, control falls
  off the end of that (now non-void) lambda with no `return` at all. That's UB,
  reproducible as a compiler-sanitizer trap, not a safe silent discard — see
  [DESIGN_NOTES.md](docs/DESIGN_NOTES.md) item 5 for the verified repro. Debug
  builds (`ILP_TYPECHECK_ENABLED`, on by default without `NDEBUG`) add a second net
  for the one case that doesn't hit the UB path — every outer iteration's inner
  search matching, so the value is silently discarded instead of crashing: they now
  abort with a message naming the mixed-API cause. That's a detection net for the
  narrower case, not a fix for the UB itself. Don't mix the two APIs; use nested
  `for_loop` calls instead and propagate explicitly (extract the inner result into a
  local, then call `ctrl.return_with(that_local)` on the outer ctrl).
- **An intervening non-ILP callback** (e.g. an `ILP_FOR` inside a `std::for_each`
  lambda inside an outer `ILP_FOR`). The value still propagates once the inner
  `ILP_FOR` completes, but the enclosing algorithm (`std::for_each`, etc.) finishes
  its own remaining iterations first — the return is deferred, not immediate.

---

## When to Use ILP

**Use ILP_FOR for loops with early exit** (`break`, `continue`, `return`). `#pragma unroll` will unroll these, but the per-iteration bounds checks it inserts eat the benefit. `ILP_FOR` skips that overhead (~1.5x speedup).

> **GCC note:** a loop body with two or more *independent* predicates (e.g. skip-if-even, then match-if-over-threshold) can hit a branch-misprediction cliff on GCC through the macro expansion. Order the most-selective condition first, or mark the enclosing function `ILP_FLATTEN`. See the GCC predicate-order caveat in [docs/PRAGMA_UNROLL.md](docs/PRAGMA_UNROLL.md).

**Skip ILP for straight-line loops with no early exit.** The auto-vectorizer handles those well on its own, and the simple, pragma, and ILP versions almost always compile to the same assembly. Using `ILP_FOR` there is harmless — in most of my tests the code was identical — just unnecessary.

```cpp
// Use ILP_FOR - early exit benefits from fewer bounds checks
ILP_FOR_AUTO(auto i, 0, n, Search, int) {
    if (data[i] == target) ILP_BREAK;
} ILP_END;

// Skip ILP - compiler auto-vectorizes loops without break
int sum = std::accumulate(data.begin(), data.end(), 0);
```

### Where ilp_for loses

`ilp_for` is not the right tool for a *trivially-vectorizable* search - a loop whose
exit condition is a simple comparison over contiguous data (`std::find`, `memchr`,
"first index where `x == target`"). Those are far better served by SIMD chunked
scanning: load a vector of elements, compare them all at once, and use a `movemask`
(or equivalent) to find the first hit, exactly as a tuned `memchr`/`std::find`
implementation does. That processes 16/32/64 elements per branch instead of unrolling
scalar comparisons.

`ilp_for` targets the case the auto-vectorizer and `movemask` tricks *can't* reach:
early-exit loops whose bodies aren't vectorizable (branchy per-element work, function
calls, dependency chains, irregular control flow), where the win comes from breaking
dependency chains and removing per-iteration bounds checks rather than from packing
data into vector registers.

See [docs/PERFORMANCE.md](docs/PERFORMANCE.md) for benchmarks and [docs/PRAGMA_UNROLL.md](docs/PRAGMA_UNROLL.md) for why pragma doesn't help.

The underlying gap this library works around — SCEV falling back to a bounds check
per element instead of per unrolled block for early-exit loops — is a missed
optimization, not a fundamental limit of what compilers can do; see
[Could This Be Fixed Upstream?](docs/PRAGMA_UNROLL.md#could-this-be-fixed-upstream)
for why, and what it would mean for this library if a compiler ever closed it.

---

## Advanced

### CPU Architecture, Portability and _AUTO functions

The `_AUTO` macros pick their unroll factors from a CPU profile. With no profile defined they fall back to conservative defaults — fine, but if you're chasing the last few percent, or building one codebase for several machines, tell them which silicon they're on:

```bash
clang++ -std=c++20 -DILP_CPU_SKYLAKE      # Intel Skylake
clang++ -std=c++20 -DILP_CPU_ALDERLAKE    # Intel Alder Lake
clang++ -std=c++20 -DILP_CPU_APPLE_M1     # Apple M1
clang++ -std=c++20 -DILP_CPU_ZEN5         # AMD Zen 4/5
```

Each profile cites the sources its instruction-timing data came from, so the numbers are checkable rather than folklore.
If you build a profile for a new architecture, send it my way and I'll get it added.

### Debugging

If you need to debug your loop logic, you can disable ILP entirely:

```bash
clang++ -std=c++20 -DILP_MODE_SIMPLE -O0 -g mycode.cpp
```

`ILP_MODE_SIMPLE` does **not** change what the macros expand to — every `ILP_FOR`
block still lowers to the same body lambda taking a `ctrl` parameter, at every
nesting depth, exactly like the default build. What it changes is `ilp::default_mode`
(the runtime unrolling strategy the macros dispatch on): with it defined, every loop
runs the remainder-only path — one bounds check per iteration, no unrolling — the
simplest code path to single-step through.

| ILP Macro | Meaning (same in both modes) |
|-----------|----------------------|
| `ILP_CONTINUE` | skip to the next iteration (`return;` from the body lambda) |
| `ILP_BREAK` | exit the loop |
| `ILP_RETURN(x)` | return `x` from the enclosing function, at any nesting depth |
| `ILP_END` / `ILP_END_RETURN` | close the loop (must match whether the body uses `ILP_RETURN`) |

Because the macro layer is unconditional, every compile-time/runtime guarantee
(END-enforcement, debug-mode type/consumption checks) holds identically under
`ILP_MODE_SIMPLE` — nothing here is default-build-only. A bare `return;` written
directly in a loop body (rather than via `ILP_CONTINUE`) also means *continue* in
both modes now, since it's returning from the same body lambda either way.

`ILP_MODE_SIMPLE` also switches the function API's default (via `ilp::default_mode`),
so `ilp::for_loop(...)` calls in the same translation unit degrade the same way.
For a per-loop alternative that doesn't require a global define — e.g. to de-ILP a
single loop while leaving the rest of the file unrolled — see
[Per-loop debug mode](#per-loop-debug-mode-function-api-exclusive) in the
[Macro-free API](#macro-free-api) section.

### LoopType Reference

When using `_AUTO` variants, you **must** specify a 'LoopType' to auto-select the optimal unroll factor:

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

**The basic principle:** pick the LoopType for your loop's **bottleneck operation** — the slowest or most congested one.

**Why?** The optimal unroll factor follows `N ≈ Latency × Throughput`: enough independent operations in flight to hide the bottleneck's latency and keep its execution unit saturated.

**Mixed operations (adds and multiplies in the same body):**

1. **Identify the critical path** — dependent operations form a chain; independent ones overlap for free
2. **Pick the slowest operation on that path:**
   - `acc += data[i] * weight[i]` → This is FMA, use `DotProduct`
   - `acc += data[i]; acc *= factor;` → Multiply is slower, use `Multiply`
   - Mostly adds with occasional multiply → `Sum`
   - Mostly multiplies with occasional add → `Multiply`

3. **Early exit trumps everything:**
   - With `ILP_BREAK` or `ILP_RETURN` in the body, branch prediction is usually the real bottleneck
   - Use `Search`, whatever the arithmetic inside

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

### clang-tidy LoopType analysis

If all else fails, the `ilp-loop-analysis` clang-tidy check recognizes common loop patterns and suggests the right LoopType for you — with `--fix` it will even rewrite the loop. Still beta-quality, but worth a run. See [tools/clang-tidy/](tools/clang-tidy/README.md).

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

An experiment: what if a library shipped a formal, machine-readable knowledge base that LLMs could query, instead of inferring the rules from documentation and source? Prose docs have gaps and ambiguities, and models fill them with plausible-sounding falsehoods. A graph of precise contracts — each node grounded, link by link, all the way down to the C/C++ standard — leaves much less room for that.

### How it works

The [`knowledge/ilp_for_axioms.toml`](knowledge/ilp_for_axioms.toml) file contains 1000+ formal axioms auto extracted from the codebase covering:
- Macro preconditions (e.g., "N must be a compile-time constant expression")
- Type constraints (e.g., "loop variable must be integral")
- Runtime invariants (e.g., "start <= end for valid loop range")
- Template SFINAE conditions and concept requirements
- Violation behavior (compile error, runtime error, undefined behavior)

When you ask Claude Code or another AI tool about `ilp_for`, it can consult these specifications to generate correct code and explain *why* a given pattern fails — a precise statement of the library's contracts, rather than a hope that the model remembers them.

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
