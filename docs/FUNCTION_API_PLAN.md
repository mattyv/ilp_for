# Function API Parity Plan — Mode Switch + Ergonomics + README Repositioning

**Status:** Implemented (see `ilp_for/detail/mode.hpp`, `ForCtrl::break_loop`/
`return_with`, `tests/correctness/test_function_api.cpp`, and the README
"Macro-free API" section). Kept for design rationale. Companion to
[DESIGN_NOTES.md](DESIGN_NOTES.md) (which covers macro-layer sharp edges; this
doc covers the function-layer feature work).

Known wart discovered during implementation: `for_loop`/`for_loop_range` return
a `[[nodiscard]]` `ForResult` even for break/continue-only bodies, forcing
`[[maybe_unused]] auto r = ...` at call sites. Fixed by `ilp::for_each` in
[END_ENFORCEMENT_PLAN.md](END_ENFORCEMENT_PLAN.md).

**Motivation:** The loudest predictable criticism of this library is the macro syntax.
The counter is that a full non-macro API already exists (`ilp::for_loop` and friends in
`detail/loops_ilp.hpp`) — it just lacks two things the macros have, plus documentation:

1. The `ILP_MODE_SIMPLE` compile-time switch (macros can degrade to plain `for` loops;
   the functions currently cannot).
2. Ergonomic control flow (`ForCtrl` is mutated via raw fields: `ctrl.ok = false`,
   `ctrl.storage.set(x)` — fine for macro expansions, hostile as a public API).
3. Any presence in the README.

The macros remain the headline API — they are less verbose and easier to remember.
The function API is the escape hatch for codebases whose style guides ban macros.

**Non-goals (explicitly out of scope for this change):**

- The `std::optional`-returning "search" sugar (`ilp::find`-style bodies with no ctrl
  param). Separate follow-up; `for_loop_range_ret_simple_impl` is the seed for it.
- Compile-time `ILP_END`/`ILP_END_RETURN` enforcement (DESIGN_NOTES item 2).
- The SBO type-pun fix (DESIGN_NOTES item 3).

---

## Part 1 — Mode switch for the function API

### Design

Add a mode enum and a global default derived from the existing define:

```cpp
namespace ilp {
    enum class Mode { Unrolled, Simple };

#ifdef ILP_MODE_SIMPLE
    inline constexpr Mode default_mode = Mode::Simple;
#else
    inline constexpr Mode default_mode = Mode::Unrolled;
#endif
}
```

Location: a new tiny header `ilp_for/detail/mode.hpp` (included by `ctrl.hpp` or
directly by `loops_common.hpp`), so both the impl headers and user code can see it.

Every public function template gains a `Mode` parameter **after** the existing
parameters it defaults from, so all existing call sites compile unchanged:

```cpp
template<std::size_t N = 4, Mode M = default_mode, std::integral T, typename F>
    requires detail::ForUntypedCtrlBody<F, T>
ForResult for_loop(T start, T end, F&& body);
```

Same treatment for the whole family in `loops_ilp.hpp`:

- `for_loop`, `for_loop_typed` (M after N)
- `for_loop_range`, `for_loop_range_typed` (M after N)
- `for_loop_range_ret_simple` (M after N)
- `for_loop_auto`, `for_loop_typed_auto`, `for_loop_range_auto`,
  `for_loop_range_typed_auto` (M after LoopType `LT`)

### Implementation

Inside each `detail::*_impl`, branch with `if constexpr`. Simple mode runs **only the
existing remainder/tail loop** — that loop already has exactly plain-`for` semantics
(one bounds check per iteration, ctrl checked after each body call). Example for
`for_loop_untyped_impl`:

```cpp
template<std::size_t N, Mode M, std::integral T, typename F>
    requires ForUntypedCtrlBody<F, T>
ForResult for_loop_untyped_impl(T start, T end, F&& body) {
    validate_unroll_factor<N>();   // keep: mode switching must not hide a bad N
    ForCtrl ctrl;
    T i = start;

    if constexpr (M == Mode::Unrolled) {
        for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
            for (std::size_t j = 0; j < N; ++j) {
                body(i + static_cast<T>(j), ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return ForResult{ctrl.return_set, std::move(ctrl.storage)};
            }
        }
    }

    for (; i < end; ++i) {          // remainder loop == the entire loop in Simple mode
        body(i, ctrl);
        if (!ctrl.ok) [[unlikely]]
            return ForResult{ctrl.return_set, std::move(ctrl.storage)};
    }

    return ForResult{false, {}};
}
```

Apply the same pattern to all six `*_impl` functions in `loops_ilp.hpp`. For
`for_loop_range_impl` (the ctrl-less/`LoopCtrl<void>` overload) both of its internal
branches get the same treatment.

Notes:

- Keep `validate_unroll_factor<N>()` unconditional so an invalid N fails to compile
  in both modes — otherwise flipping `ILP_MODE_SIMPLE` on/off changes what compiles.
- No changes to the macro layer. When `ILP_MODE_SIMPLE` is defined the macros expand
  via `macros_simple.hpp` to literal `for` loops and never call these functions; the
  define reaches function-API users through `default_mode`. The two layers stay
  consistent with zero coordination.
- `[[unlikely]]` and the early-return-on-ctrl structure are preserved verbatim in the
  Simple path (it *is* the existing tail loop, unmodified).

### What this buys over the macros

Per-loop override, which a translation-unit-wide `#define` cannot express:

```cpp
// Whole file built normally, but de-ILP just this loop while debugging it:
auto r = ilp::for_loop<4, ilp::Mode::Simple>(0, n, [&](int i, auto& ctrl) { ... });
```

Document this as a function-API-exclusive feature.

### Honest caveat for the docs

Macro simple mode lowers `ILP_BREAK` to a literal `break` inside a literal `for` —
maximally plain single-stepping at `-O0`. Function-API simple mode still invokes the
body through a lambda, so there is one extra stack frame when stepping. Same
semantics, marginally less pristine debugger experience. One sentence in the README,
no design change.

---

## Part 2 — `ForCtrl` / `ForCtrlTyped` ergonomics

Add member functions so lambda bodies never touch raw fields. Match the naming
already established by `LoopCtrl` (`break_loop`, `return_with`) for consistency:

```cpp
struct ForCtrl {
    bool ok = true;
    bool return_set = false;
    SmallStorage storage;

    ILP_ALWAYS_INLINE void break_loop() { ok = false; }

    template<typename T>
    ILP_ALWAYS_INLINE void return_with(T&& val) {
        storage.set(static_cast<T&&>(val));
        return_set = true;
        ok = false;
    }
};
```

Same two methods on `ForCtrlTyped<R>` (where `return_with` takes anything
constructible into `R`, mirroring `TypedStorage::set`).

- Both return `void` so bodies can write the one-liner `return ctrl.break_loop();`
  (break-and-exit-body in a single statement, mirroring `ILP_BREAK`'s `return;`).
- Keep the public fields — the macros in `ilp_for.hpp` write them directly and must
  not change in this PR. Optionally migrate `ILP_BREAK`/`ILP_RETURN` to call the new
  methods (identical codegen, single source of truth); safe but verify with the
  assembly examples before/after.
- A bare `return;` from the body lambda means *continue*. In the macro layer this is
  a trap (DESIGN_NOTES item 1); in a lambda API it is the natural, idiomatic meaning.
  Say so in the docs — it reads as a point in the function API's favor.

Canonical example this enables (use in README and tests):

```cpp
auto r = ilp::for_loop<4>(0, n, [&](int i, auto& ctrl) {
    if (data[i] < 0)  return ctrl.break_loop();     // ILP_BREAK
    if (data[i] == 0) return;                       // ILP_CONTINUE
    if (data[i] == target) return ctrl.return_with(i);  // ILP_RETURN(i)
    sum += data[i];
});
if (r) return *std::move(r);   // propagate early return, same as ILP_END_RETURN
```

---

## Part 3 — README repositioning

Framing rule: **macros stay the headline**. They are the less verbose, easier to
remember syntax and every existing example keeps using them. The function API is
presented as the supported escape hatch, not the recommended path.

Changes:

1. In "How It Works", after the first `ILP_FOR` example, add one sentence + link:
   *"Everything the macros do is sugar over a plain function API — if your codebase
   bans macros, use that directly ([Macro-free API](#macro-free-api))."*
   This single sentence defuses the "macros!" objection at the point where readers
   first see one.

2. New README section **"Macro-free API"** (after API Reference), containing:
   - The canonical example from Part 2, side by side with its macro equivalent.
   - An equivalence table:

     | Macro | Function API |
     |-------|--------------|
     | `ILP_FOR(auto i, 0, n, 4) {...} ILP_END` | `ilp::for_loop<4>(0, n, [&](auto i, auto& ctrl){...})` |
     | `ILP_FOR_AUTO(auto i, 0, n, Search, int)` | `ilp::for_loop_auto<int, ilp::LoopType::Search>(0, n, ...)` |
     | `ILP_FOR_RANGE(auto&& v, r, 4)` | `ilp::for_loop_range<4>(r, [&](auto&& v, auto& ctrl){...})` |
     | `ILP_FOR_T(Result, ...)` | `ilp::for_loop_typed<Result, 4>(...)` |
     | `ILP_BREAK` | `return ctrl.break_loop();` |
     | `ILP_CONTINUE` | `return;` |
     | `ILP_RETURN(x)` | `return ctrl.return_with(x);` |
     | `ILP_END_RETURN` | `if (r) return *std::move(r);` |

   - The per-loop `Mode` override example, labeled as function-API-exclusive.
   - The debugger-frame caveat from Part 1.

3. In the "Debugging" section, note that `ILP_MODE_SIMPLE` also switches the function
   API (via `ilp::default_mode`), and show the per-loop override as the alternative
   that doesn't require a global define.

---

## Part 4 — Tests

1. New `tests/correctness/test_function_api.cpp`:
   - break/continue/return-with via the new `ForCtrl` methods, untyped and typed,
     index and range variants, empty ranges, N > length, exit-in-remainder cases —
     mirror the shape of the existing `test_for_loops.cpp` coverage.
   - Explicit `Mode::Simple` and `Mode::Unrolled` instantiations of the same cases,
     asserting identical results (the two modes must be observationally equivalent).
   - A `static_assert(ilp::default_mode == ...)` check in each build flavor.
2. `tests/test_all_modes.sh` already builds with and without `ILP_MODE_SIMPLE`; the
   new test file rides that matrix for free. Verify it is added to the correctness
   CMake target.
3. If `ILP_BREAK`/`ILP_RETURN` are migrated to call the new methods (Part 2 option),
   run the godbolt/assembly examples before and after to confirm identical codegen.

---

## Implementation order

1. `mode.hpp` + thread `Mode` through `loops_ilp.hpp` (Part 1) — mechanical.
2. `ForCtrl`/`ForCtrlTyped` methods (Part 2) — small, isolated in `ctrl.hpp`.
3. Tests (Part 4) — proves 1+2 before touching docs.
4. README (Part 3) — last, so examples are copy-pasted from passing tests.
