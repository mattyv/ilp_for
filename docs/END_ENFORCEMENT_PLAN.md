# Compile-Time ILP_END / ILP_END_RETURN Enforcement — Implementation Plan

**Status:** Implemented (see `ilp::EachCtrl`, `ilp::for_each`/`for_each_range` in
`ilp_for/detail/loops_ilp.hpp`, the tag-dispatch macros in `ilp_for.hpp`,
`tests/compile_fail/`, `tests/correctness/test_end_enforcement.cpp`, and the
README's "ILP_END vs ILP_END_RETURN" and "Macro-free API" updates). Verified on
GCC 13.3 and Clang 18.1, both build modes, plus a manual ASan+UBSan subset run.

One incidental, pre-existing (not introduced or fixed by this work) limitation
was found while adding test coverage: an `ILP_RETURN` inside a loop nested
*within another loop's body* does not propagate its value out of the true
enclosing function. Documented as a new finding in
[DESIGN_NOTES.md](DESIGN_NOTES.md).

**Supersedes:** [DESIGN_NOTES.md](DESIGN_NOTES.md) item 2's documentation-only
proposal. This makes the `ILP_RETURN`-with-`ILP_END` mismatch a **compile
error**, deletes the runtime `abort()` path entirely, and — as a bonus — fixes
the `[[nodiscard]]` ergonomic wart in the function API flagged during the
FUNCTION_API_PLAN implementation (break/continue-only `ilp::for_loop` calls
forcing `[[maybe_unused]] auto r = ...`).

---

## The mechanism

The opening macro (`ILP_FOR`) and the closing macro (`ILP_END` /
`ILP_END_RETURN`) jointly form **one function call** — the opener writes
`detail::macro_for<N>(start, end, [&](...)` and the closer writes `); }();`.
That means the closing macro can append a **tag argument** to the call, and
overload resolution on the tag decides which ctrl type the body lambda is
instantiated with:

- `ILP_END` appends `, ::ilp::detail::end_tag_t{}` → the tag overload runs the
  loop with a **break-only ctrl** whose `return_with` is poisoned with a
  `static_assert`, and returns a trivial non-`[[nodiscard]]` `NoResult`.
- `ILP_END_RETURN` appends `, ::ilp::detail::end_return_tag_t{}` → the tag
  overload runs with `ForCtrl` (as today) and returns `ForResult`.

For this to work, the opener declares the ctrl lambda parameter as `auto&`
instead of a concrete type, and drops the explicit `-> ::ilp::ForResult`
trailing return on the IIFE (deduced instead). The body is a generic lambda, so
member lookup on `ilp_detail_ctrl` happens at instantiation — which is exactly
when the tag has already chosen the ctrl type. A body containing `ILP_RETURN`
instantiated against the break-only ctrl hits the poisoned `return_with`:

```
error: static assertion failed: ILP_RETURN was used inside a loop closed with
ILP_END. Change ILP_END to ILP_END_RETURN in the enclosing function.
```

No new opening macros, no API split: every currently-**correct** program
compiles unchanged. Only the mismatch — which today compiles and aborts at
runtime, and only on inputs that actually execute the return — becomes ill-formed.

### Validated prototype expansion (reference for the implementer)

```cpp
#define ILP_FOR(loop_var_decl, start, end, N)                                  \
    if ([[maybe_unused]] auto ilp_detail_ret = [&]() {                         \
        [[maybe_unused]] auto ilp_detail_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::detail::macro_for<N>(start, end,                         \
            [&](loop_var_decl, [[maybe_unused]] auto& ilp_detail_ctrl)

#define ILP_END , ::ilp::detail::end_tag_t{});                                 \
    }                                                                          \
    ();                                                                        \
    false) {}                                                                  \
    else(void) 0

#define ILP_END_RETURN , ::ilp::detail::end_return_tag_t{});                   \
    }                                                                          \
    (); ilp_detail_ret)                                                        \
    return *std::move(ilp_detail_ret);                                         \
    else(void) 0
```

Notes from the prototype:
- `if (init; false)` produces no warnings on GCC/Clang `-Wall -Wextra`. MSVC's
  C4127 exempts literal conditions since VS2015, so `false` is safe there too —
  but verify in CI when MSVC coverage exists.
- Nested loops (inner `ILP_END` inside outer `ILP_END_RETURN` body) work; the
  pre-existing `-Wshadow` caveat (DESIGN_NOTES item 4) is unchanged.
- The lambda's return type is deduced from its single `return` statement, so
  dropping the explicit `-> ForResult` annotation is safe.

---

## Changes by file

### 1. `ilp_for/detail/ctrl.hpp`

**Add** (in `ilp::detail` unless noted):

```cpp
template<typename T>
inline constexpr bool always_false = false;

struct end_tag_t {};
struct end_return_tag_t {};

// Returned by the ILP_END (no-return) macro path. Deliberately NOT [[nodiscard]].
struct NoResult {};
```

And a break-only ctrl, in namespace `ilp` with a public name since function-API
users may spell it in non-generic lambdas (suggested name `EachCtrl`):

```cpp
struct EachCtrl {
    bool ok = true;

    ILP_ALWAYS_INLINE void break_loop() { ok = false; }

    // Poison: returning a value is impossible in this loop flavor.
    template<typename T>
    void return_with(T&&) {
        static_assert(detail::always_false<T>,
            "This loop cannot return a value. "
            "If using macros: ILP_RETURN was used inside a loop closed with ILP_END - "
            "change ILP_END to ILP_END_RETURN in the enclosing function. "
            "If using ilp::for_each: use ilp::for_loop instead.");
    }
};
```

**Delete:** `detail::ilp_end_with_return_error()` and the now-unused
`#include <cstdio>` / `#include <cstdlib>` (verify nothing else in the header
uses them — as of writing, nothing does).

**Change:** none to `ForCtrl`/`ForCtrlTyped`/`ForResult`/`ForResultTyped` —
including the MSVC Proxy `#ifdef` branches and the `[[nodiscard]]` attributes.

### 2. `ilp_for/detail/loops_ilp.hpp`

**Add a break-only loop impl** (same main+remainder shape as
`for_loop_untyped_impl`, with the `Mode` template parameter threaded exactly
like the FUNCTION_API_PLAN work — `if constexpr (M == Mode::Unrolled)` guards
the block loop):

```cpp
template<std::size_t N, Mode M, std::integral T, typename F>
    requires std::invocable<F, T, EachCtrl&>
void for_each_impl(T start, T end, F&& body);           // EachCtrl, returns void

template<std::size_t N, Mode M, std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, EachCtrl&>
void for_each_range_impl(Range&& range, F&& body);
```

**Add public function-API entry points** (this is the `[[nodiscard]]`-wart fix —
document them as the right choice for break/continue-only loops):

```cpp
template<std::size_t N = 4, Mode M = default_mode, std::integral T, typename F>
void for_each(T start, T end, F&& body);

template<std::size_t N = 4, Mode M = default_mode, std::ranges::random_access_range Range, typename F>
void for_each_range(Range&& range, F&& body);
```

Body signature: `[&](auto i, auto& ctrl)` with `ctrl.break_loop()` available and
`ctrl.return_with(x)` poisoned (compile error pointing to `for_loop`).

**Add macro entry points in `ilp::detail`** — one per opener macro, each with
two tag overloads. The `end_return_tag_t` overloads delegate to the existing
impls verbatim; the `end_tag_t` overloads delegate to `for_each_impl` /
`for_each_range_impl` and return `NoResult{}`:

| Detail function | `end_tag_t` overload | `end_return_tag_t` overload |
|---|---|---|
| `macro_for<N>(start, end, body, tag)` | `for_each_impl<N, default_mode>` → `NoResult` | `for_loop_untyped_impl<N, default_mode>` → `ForResult` |
| `macro_for_range<N>(range, body, tag)` | `for_each_range_impl` → `NoResult` | `for_loop_range_untyped_impl` → `ForResult` |
| `macro_for_auto<ElemT, LT>(...)` | as above with `optimal_N<LT, ElemT>` | as above |
| `macro_for_range_auto<ElemT, LT>(...)` | " | " |
| `macro_for_typed<R, N>(...)` | `for_each_impl` (R unused) → `NoResult` | `for_loop_typed_impl<R, N, ...>` → `ForResultTyped<R>` |
| `macro_for_range_typed<R, N>(...)` | " | `for_loop_range_typed_impl` |
| `macro_for_typed_auto<ElemT, R, LT>(...)` | " | " |
| `macro_for_range_typed_auto<ElemT, R, LT>(...)` | " | " |

(A `_T` loop closed with plain `ILP_END` and no `ILP_RETURN` in the body is
pointless but legal today; keep it legal — hence the typed×`end_tag_t` cells
delegate to the untyped for_each path.)

Macro entries always use `default_mode` — when `ILP_MODE_SIMPLE` is defined the
ILP macros aren't compiled at all (`macros_simple.hpp` takes over), so no extra
Mode plumbing is needed at the macro layer.

### 3. `ilp_for.hpp` (macro layer)

- All 8 opener macros: drop the `-> ::ilp::ForResult` / `-> ::ilp::ForResultTyped<type>`
  trailing return types (deduced), change the ctrl parameter from a concrete
  type to `[[maybe_unused]] auto& ilp_detail_ctrl`, and call the matching
  `::ilp::detail::macro_for*` entry. Keep the `For_Context_USE_ILP_END` local
  (missing-END detection) unchanged.
- `ILP_END`: new expansion per the prototype above — tag + `false` condition.
  The `has_return ? (ilp_end_with_return_error(), false) : false` ternary is
  deleted along with the function itself.
- `ILP_END_RETURN`: unchanged except for the prepended tag argument.
- `ILP_BREAK` → `do { ilp_detail_ctrl.break_loop(); return; } while (0)` and
  `ILP_RETURN(x)` → `do { ilp_detail_ctrl.return_with(x); return; } while (0)`.
  Both methods exist on `ForCtrl`/`ForCtrlTyped` (added by the FUNCTION_API
  work) and on `EachCtrl` (`return_with` poisoned — that's the enforcement
  point). `ILP_ALWAYS_INLINE` on the methods keeps codegen identical; spot-check
  one godbolt example's assembly before/after to confirm.
- Update the "Macro Design Notes" comment block to describe the tag mechanism
  (the ternary/abort description it currently gives becomes wrong).
- `macros_simple.hpp`: **no change**. In simple mode `ILP_RETURN` is a plain
  `return x` in a plain loop, so the mismatched pairing is semantically fine
  there and cannot be (and needn't be) detected. Default-mode builds — which CI
  always runs — catch it. Note this asymmetry in the README.

---

## Tests

### 1. Compile-fail harness (new)

`tests/compile_fail/` with a driver script, run only in default mode (in
`ILP_MODE_SIMPLE` the mismatch legitimately compiles):

- `mismatch_end.cpp` — `ILP_RETURN` inside `ILP_FOR ... ILP_END`. Must fail;
  stderr must contain `"change ILP_END to ILP_END_RETURN"`.
- `mismatch_end_typed.cpp` — same with `ILP_FOR_T`. Must fail with the message.
- `for_each_return_with.cpp` — `ilp::for_each` body calling
  `ctrl.return_with(x)`. Must fail; stderr must contain `"use ilp::for_loop"`.
- `control_ok.cpp` — a correct `ILP_END_RETURN` pairing plus a correct
  `for_each`. Must **succeed** (proves the harness isn't passing vacuously).

Driver: `check_compile_fail.sh` compiling each case with the same
`-std=c++20 -I../..` flags, asserting exit status and grepping the message.
Wire as a CMake custom target (`compile-fail-tests`) and invoke it from
`test_all_modes.sh` in the default-mode leg.

### 2. Runtime tests

- Extend `tests/correctness/test_function_api.cpp`: `for_each` and
  `for_each_range` — basic sum, `break_loop`, bare-`return`-as-continue, empty
  range, explicit `Mode::Simple`/`Mode::Unrolled` override. These calls are
  plain statements (no `[[maybe_unused]] auto r =`), which itself demonstrates
  the wart fix; the `-Wall -Wextra` build enforces warning-freedom.
- Existing suite must pass unchanged in both modes — it contains only correct
  pairings, so it doubles as the no-breakage regression proof.
- Repeat the manual ASan+UBSan subset run from the FUNCTION_API implementation.

---

## Documentation

- **README API Reference**: replace "Always end with `ILP_END`. If using
  `ILP_RETURN`, use `ILP_END_RETURN` instead." with the same rule plus:
  mismatches are a **compile-time error** (with the simple-mode asymmetry
  noted). This is a bragging point — phrase it as one.
- **README Macro-free API section**: add `for_each`/`for_each_range` with a
  sentence ("break/continue-only loops: use `for_each`, which returns `void`"),
  update the equivalence table (`ILP_FOR`+`ILP_END` row now maps to `for_each`),
  and delete the `[[nodiscard]]`/`[[maybe_unused]]` caveat paragraph.
- **DESIGN_NOTES.md item 2**: append a status update: resolved by compile-time
  enforcement (brief mechanism description + pointer to this plan); the
  runtime-abort documentation above it becomes historical.
- `docs/FUNCTION_API_PLAN.md`: mark Status as implemented (done in the same
  commit series).
- `godbolt_examples/*.cpp` inline a stale snapshot of the library (they still
  contain `ilp_end_with_return_error` and predate `Mode`). They remain
  self-contained and compilable, so regenerating them is a **separate follow-up**,
  not part of this change.

---

## Behavior changes to call out in the commit message

1. `ILP_RETURN` + `ILP_END` no longer compiles (previously compiled and aborted
   at runtime — and only on inputs that executed the return). This is the point
   of the change; any code it breaks was already broken.
2. The runtime abort path (`ilp_end_with_return_error`) is deleted; the library
   no longer pulls in `<cstdio>`/`<cstdlib>`.
3. New public API: `ilp::for_each`, `ilp::for_each_range`, `ilp::EachCtrl`.

## Implementation order

1. `ctrl.hpp`: tags, `NoResult`, `EachCtrl`, delete the abort helper — isolated.
2. `loops_ilp.hpp`: `for_each_impl`/`for_each_range_impl`, public `for_each`/
   `for_each_range`, then the eight `macro_for*` tag-dispatch entries.
3. `ilp_for.hpp`: rewrite the openers/closers/control macros against the new
   entries. Existing test suite green in both modes before proceeding.
4. Compile-fail harness + `test_function_api.cpp` additions.
5. Docs last, examples copy-pasted from passing tests.
