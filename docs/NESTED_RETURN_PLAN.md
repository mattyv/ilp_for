# Nested ILP_RETURN Propagation — Implementation Plan

**Status:** Implemented (see `ilp::detail::propagate_return`/`ctrl_typed_return` in
`ilp_for/detail/ctrl.hpp`, the `ilp_detail_ctrl()` sentinel and updated
`ILP_END_RETURN` in `ilp_for.hpp`, `tests/correctness/test_nested_return.cpp`,
the two new `tests/compile_fail/` cases, and the README's "Nested Loops"
section). Verified on GCC 13.3 and Clang 18.1, both build modes, plus a manual
ASan+UBSan subset run — all green, no new warnings. The `-Wshadow` spot check
confirmed the sentinel itself is silent in non-nested code; the nested-loop
shadow warnings it does trigger are the pre-existing, already-tracked
DESIGN_NOTES item 4 category (unchanged by this work).

Mechanism prototype-validated against the in-repo headers on GCC 13.3 and
Clang 18.1: 2-level and 3-level nesting, typed×untyped combinations, and
control flow around nested loops all produce correct values; the identical
source produces identical results under `ILP_MODE_SIMPLE`; the new poison
case fails to compile with an actionable message; and the sentinel introduces
**zero** new `-Wshadow` warnings (GCC `-Wshadow`, Clang `-Wshadow-all`).

**Fixes:** the DESIGN_NOTES incidental finding "nested `ILP_RETURN` does not
propagate through an outer loop's body" — currently a *silent wrong answer* (the
inner return value is discarded and the function falls through to its fallback
return), flagged only by an easy-to-miss `-Wreturn-type` warning.

---

## Semantics decision: match `ILP_MODE_SIMPLE`

The spec is already decided by the simple mode. Under `ILP_MODE_SIMPLE`, nested
`ILP_FOR` loops are plain `for` loops and `ILP_RETURN` is a plain `return`, so an
inner return **does** escape the entire enclosing function. Default mode silently
disagrees today — a mode-semantics divergence worse than DESIGN_NOTES item 1.
After this change, `ILP_RETURN` means the same thing at any nesting depth in both
modes: *return this value from the enclosing C++ function*, with the constraint
(compile-time enforced) that **every enclosing `ILP_FOR` on the path out must be
closed with `ILP_END_RETURN`** — because each of them carries the value one level
outward.

## The mechanism

`ILP_END_RETURN`'s expansion (`return *std::move(ilp_detail_ret);`) sits textually
in whatever scope contains the macro invocation:

- at **function scope**, that's the real function — the Proxy conversion works and
  behavior must stay as-is;
- **nested inside another loop's body**, that scope is the outer loop's body
  lambda — where the outer loop's `ilp_detail_ctrl` parameter is visible by
  unqualified name lookup.

So a single new expansion can dispatch on what `ilp_detail_ctrl` names at the
expansion point, provided the name also resolves at plain function scope. That is
arranged with a global fallback:

```cpp
// ilp_for.hpp, global namespace (before the macro definitions)
// Fallback target for unqualified `ilp_detail_ctrl` lookup when ILP_END_RETURN is
// expanded at plain function scope (not nested inside another ILP body lambda,
// where the body's ctrl parameter shadows this). Declared as a *function* rather
// than an object so body-lambda parameters shadowing it stay outside -Wshadow's
// variable-shadowing warnings (verified silent on GCC -Wshadow and Clang
// -Wshadow-all).
inline void ilp_detail_ctrl() {}
```

`ILP_END_RETURN` becomes (only the `return` line changes):

```cpp
#define ILP_END_RETURN , ::ilp::detail::end_return_tag_t{});  \
    }                                                          \
    (); ilp_detail_ret)                                        \
    return ::ilp::detail::propagate_return(ilp_detail_ret, ilp_detail_ctrl); \
    else(void) 0
```

And the dispatcher (in `ctrl.hpp`, `ilp::detail`, after the ctrl/result types):

```cpp
template<typename T>
struct ctrl_typed_return { using type = void; static constexpr bool is_typed = false; };
template<typename R>
struct ctrl_typed_return<ForCtrlTyped<R>> { using type = R; static constexpr bool is_typed = true; };

// Dispatch for ILP_END_RETURN. `outer` is whatever unqualified lookup finds for
// `ilp_detail_ctrl` at the macro expansion point:
//  - global sentinel function  -> top level: return the Proxy (unchanged behavior)
//  - ForCtrl / ForCtrlTyped<R> -> nested in an outer ILP_END_RETURN loop:
//                                 propagate the value into the outer ctrl, return void
//  - EachCtrl                  -> nested in an outer ILP_END loop: poison
template<typename Res, typename Ctrl>
ILP_ALWAYS_INLINE decltype(auto) propagate_return(Res& r, Ctrl& outer) {
    using C = std::remove_cvref_t<Ctrl>;
    if constexpr (std::is_same_v<C, EachCtrl>) {
        static_assert(always_false<Ctrl>,
            "ILP_RETURN inside this nested loop cannot escape: the enclosing "
            "ILP_FOR is closed with ILP_END. Change the enclosing loop's "
            "ILP_END to ILP_END_RETURN so the value can propagate out.");
    } else if constexpr (std::is_same_v<C, ForCtrl>) {
        if constexpr (std::is_same_v<Res, ForResult>) {
            outer.storage = r.storage;   // type-erased byte copy, same pun contract
            outer.return_set = true;
            outer.ok = false;
        } else {
            outer.return_with(r.storage.extract());   // typed inner -> untyped outer
        }
    } else if constexpr (ctrl_typed_return<C>::is_typed) {
        using R2 = typename ctrl_typed_return<C>::type;
        if constexpr (std::is_same_v<Res, ForResult>) {
            outer.return_with(r.storage.template extract<R2>());
        } else {
            outer.return_with(r.storage.extract());   // typed inner -> typed outer
        }
    } else {
        return *r;   // top level: Proxy converts to the function's return type
    }
}
```

Why each piece is safe (all prototype-verified):

- **Deduction.** `propagate_return` is a template, so `if constexpr` discards
  untaken branches; the nested overloadings return `void`, so the outer body
  lambda deduces `void` alongside `ILP_BREAK`/`ILP_CONTINUE`'s bare `return;`.
  The `-Wreturn-type` fall-off-the-end warning from the broken pattern disappears.
- **Lifetime.** `ilp_detail_ret` is passed by lvalue reference; at top level the
  returned Proxy references `ilp_detail_ret.storage`, which lives in the `if`
  init-statement scope through the whole return statement — identical lifetime to
  today's `return *std::move(ilp_detail_ret)`.
- **Transitive enforcement.** Each level's `ILP_END_RETURN` propagates into *its*
  visible outer ctrl. If any enclosing loop is closed with `ILP_END`, its ctrl is
  `EachCtrl` and the poison branch fires — the END-enforcement mechanism extends
  through nesting for free, with a message naming the exact fix.
- **Storage semantics matrix** (inner result → outer ctrl):

  | | outer `ForCtrl` (untyped) | outer `ForCtrlTyped<R2>` | outer `EachCtrl` |
  |---|---|---|---|
  | inner `ForResult` (untyped) | byte-copy the `SmallStorage` (trivially copyable char buffer) | `extract<R2>()` → `return_with` | compile error |
  | inner `ForResultTyped<R1>` | `extract()` (R1) → `return_with` — SBO static_assert fires with the existing "Use ILP_FOR_T" message if R1 > 8 bytes, correctly telling the user to make the *outer* loop `ILP_FOR_T` too (verified) | `extract()` → `return_with` (R1→R2 conversion) | compile error |

- **`-Wshadow`.** The sentinel is a *function*; GCC/Clang shadow warnings cover
  variables, parameters, types, and built-ins — not ordinary functions. Verified:
  single-loop TU compiles with 0 warnings under `-Wshadow`/`-Wshadow-all` with the
  new headers, same as baseline. (The pre-existing nested-loop expansion-local
  shadow warnings — DESIGN_NOTES item 4 — are unchanged and out of scope.)
- **Mode parity.** The full nested test battery produces identical results
  compiled with and without `ILP_MODE_SIMPLE` (verified).

## Changes by file

1. **`ilp_for.hpp`**: add the global sentinel (before the `ILP_MODE_SIMPLE`
   branch so it exists in both modes — harmless in simple mode); change
   `ILP_END_RETURN`'s return line; extend the "Macro Design Notes" comment block
   with the nesting/propagation mechanism.
2. **`ilp_for/detail/ctrl.hpp`**: add `ctrl_typed_return` and `propagate_return`
   (needs `<type_traits>` — already included). Prototype-exact code above.
3. **Guard (small, this plan's scope):** `SmallStorage::set` and
   `TypedStorage::set` gain a `static_assert` rejecting `ForResult::Proxy` /
   `ForResultTyped<R>::Proxy` as the stored type. Rationale: a function-API user
   hand-propagating with `ctrl.return_with(*std::move(r))` would deduce T=Proxy
   and store a *dangling reference wrapper* (8 bytes, trivially destructible — it
   passes today's asserts). Message: "Cannot store a loop-result Proxy: extract
   the value into a typed local first, or return it directly with
   `return *std::move(r);`." (Give the Proxies a member tag, e.g.
   `using ilp_is_proxy = void;`, and detect via a `requires` expression — the
   Proxy types are nested classes, awkward to name in a trait otherwise.)

## What this does NOT cover (document, don't fix)

- **Macro loop nested inside a function-API `for_loop` body.** The user's ctrl
  has whatever name they gave it, so lookup finds the global sentinel and treats
  the macro loop as top-level — the returned Proxy is swallowed by the user's
  deduced-return lambda, the same failure shape as the old bug. Function-API
  users compose returns explicitly; add one sentence to the README's macro-free
  section: "don't use the loop macros inside a `for_loop` body; use nested
  `for_loop` calls and propagate via `ctrl.return_with(...)` with an extracted
  value."
- **An intervening non-ILP lambda** (e.g. the macro loop inside a
  `std::for_each` callback inside an outer ILP body): lookup still finds the
  outer ctrl through capture, and the value propagates, but the intervening
  algorithm continues its remaining iterations before the outer loop notices
  `ok == false` — the return is deferred, not immediate. Same behavior class as
  simple mode (where the plain `return` would exit only the callback). One
  caveat sentence in the README nested-loops note.
- **The stored-type/extracted-type pun contract** (DESIGN_NOTES item 3) now also
  applies across propagation hops (untyped→untyped byte copy, untyped→typed
  `extract<R2>`). Item 3's eventual debug-mode type check must cover
  `propagate_return`'s extracts too — add a cross-reference to item 3.

## Tests

1. **Runtime — new `tests/correctness/test_nested_return.cpp`** (promote the
   prototype battery):
   - 2-level: the exact DESIGN_NOTES repro, now expecting the *correct* value —
     note it's **102** (`r=1, c=2 → 1*100+2`), not the 105 the deleted test
     claimed (that expectation was arithmetically wrong; it never mattered
     because the bug returned -1).
   - Not-found fallthrough at each depth (inner completes → outer continues).
   - 3-level nesting.
   - `ILP_FOR_T`(big type) inside `ILP_FOR_T`(same type).
   - Untyped inner inside `ILP_FOR_T`(int) outer.
   - Control flow around the nested loop: `ILP_CONTINUE`/`ILP_BREAK` in the outer
     body before/after an inner `ILP_END` loop (unchanged behavior).
   - These run in both modes via `test_all_modes.sh` — the file must produce
     identical results under `ILP_MODE_SIMPLE`, which is the mode-parity proof.
2. **Compile-fail — add to `tests/compile_fail/`:**
   - `nested_return_in_end_loop.cpp`: inner `ILP_END_RETURN` inside an outer
     `ILP_END` loop. Expect: `"Change the enclosing loop's ILP_END to
     ILP_END_RETURN"`.
   - `proxy_into_return_with.cpp`: function-API `ctrl.return_with(*std::move(r))`.
     Expect: `"Cannot store a loop-result Proxy"`.
3. **Update `tests/correctness/test_end_enforcement.cpp`**: the NOTE block
   documenting the limitation as unfixed becomes wrong — replace it with a
   pointer to `test_nested_return.cpp`, and reinstate an actual
   "ILP_END_RETURN nested inside outer loop" positive test there (with correct
   expectations).
4. **Regression:** full existing suite in both modes, both compilers, plus the
   ASan+UBSan subset run — the propagation path moves bytes between storages, so
   the sanitizer pass matters here more than usual.
5. **`-Wshadow` spot check:** compile the single-loop compile-fail control (or a
   small TU) with `-Wshadow` (GCC) / `-Wshadow-all` (Clang) and assert no *new*
   warnings vs. baseline. Fold into the compile-fail driver as a
   `// COMPILE_OK`-with-flags case if convenient, otherwise a manual check noted
   in the commit message.

## Documentation

- **README**: in Important Notes, add a "Nested loops" subsection: `ILP_RETURN`
  returns from the enclosing function at any nesting depth (matching
  `ILP_MODE_SIMPLE`); every enclosing loop must be `ILP_END_RETURN`-closed —
  enforced at compile time; the two documented caveats above.
- **DESIGN_NOTES.md**: append `**Status: resolved.**` to the nested-return
  incidental finding with a pointer to this plan (same convention as item 2's
  resolution note).
- **This file**: flip Status to implemented at the end.

## Implementation order

1. `ctrl.hpp`: `ctrl_typed_return` + `propagate_return` + the Proxy-storage
   guard — isolated, compiles standalone.
2. `ilp_for.hpp`: sentinel + `ILP_END_RETURN` change + comment block.
3. `test_nested_return.cpp` + compile-fail cases + `test_end_enforcement.cpp`
   NOTE update. Full suite green in both modes, both compilers, sanitizers.
4. Docs last, examples copy-pasted from passing tests.
