# Closing DESIGN_NOTES Items 1, 4, 5 — Implementation Plan

**Status:** Implemented. All three mechanisms landed as described below and
verified against the full test matrix (both build modes, both compilers, both
compile_fail/runtime_fail harnesses, sanitizer subset, NDEBUG asm identity). See
DESIGN_NOTES.md items 1/4/5 for the final resolution write-ups, including a
clang-tidy invocation caveat discovered during item 4's verification (not
anticipated by this plan — `#pragma GCC system_header` also affects clang-tidy's
own diagnostic suppression, fixed by a repo-root `.clang-tidy` setting
`HeaderFilterRegex`/`SystemHeaders` plus explicit `-header-filter='.*'
-system-headers` flags on invocations whose inputs live outside the tree).
A post-implementation review pass added further hardening: the abort is now
gated on a value actually being present (empty-result Proxies discard freely),
Proxy copies transfer the consume obligation (the MSVC caveat this plan's item 5
notes anticipated), the range-category breaking change of item 1 is documented
(DESIGN_NOTES item 1, README), the PCH/main-file limitation of item 4 is
documented, and the compile-fail/runtime-fail harnesses now run in CI
(`harness-tests` job) with additional `-Wshadow` probes covering macro-argument
and body-declaration shadowing.

---

## Item 1 — Bare `return;` divergence: unify the modes

### Decision

Don't patch the symptom; remove the second implementation. `ILP_MODE_SIMPLE`
already flips `ilp::default_mode` to `Mode::Simple` (mode.hpp), and the macro
entry points already honor `default_mode`. So: **delete `macros_simple.hpp` and
the `#ifdef ILP_MODE_SIMPLE` branch in `ilp_for.hpp`** — the normal macro layer
compiles unconditionally, and under `ILP_MODE_SIMPLE` every loop runs the
existing `Mode::Simple` path (remainder loop only, one bounds check per
iteration — plain-loop codegen).

The bare-`return;` divergence ceases to exist structurally: there is one macro
layer, so `return;` means *continue* in both modes, `ILP_RETURN` propagates
identically in both modes, and every compile-time guarantee extends to SIMPLE
builds.

### Prototype-verified wins

- Full **352-assertion suite passes in BOTH modes** with `macros_simple.hpp`
  gone and all `#if !defined(ILP_MODE_SIMPLE)` test guards **deleted** (9 test
  files carry them today). SIMPLE-mode coverage jumps from 108 to 352
  assertions — every ILP_RETURN / nested-propagation / END-enforcement /
  typecheck test now runs there.
- The `ILP_END`/`ILP_END_RETURN` mismatch is now a **compile error in SIMPLE
  mode too** (verified: `mismatch_end.cpp` fails with the poison message under
  `-DILP_MODE_SIMPLE`), and the SBO **type check catches puns in SIMPLE mode**
  (verified: `pun_toplevel.cpp` aborts). Both harnesses can therefore run in
  the SIMPLE leg of `test_all_modes.sh` — delete their "default mode only"
  restrictions and comments.

### Breaking changes to document (README Debugging section + changelog note)

1. Bare `return;` in a loop body now means *continue* in SIMPLE builds
   (previously: returned from the enclosing function). Anyone relying on that
   was already writing mode-divergent code; `ILP_RETURN`/`ILP_CONTINUE` are the
   portable spellings. This closes DESIGN_NOTES item 1 in the both-continue
   direction (the only implementable one).
2. SIMPLE mode no longer lowers to a literal `for` loop: single-stepping has
   one lambda frame (same caveat already documented for the function API's
   `Mode::Simple`). Codegen remains plain (no unrolling, one bounds check per
   iteration).
3. Bodies using raw `break;`/`continue;`, `goto` out of the loop, or coroutine
   keywords no longer compile in SIMPLE mode (they never compiled in default
   mode — this is divergence removal, not new restriction).

### Mechanical steps

- `ilp_for.hpp`: delete the `#ifdef ILP_MODE_SIMPLE` / `#include
  macros_simple.hpp` / `#else` / `#endif` scaffolding (macros become
  unconditional). Update the two stale comments referencing "In
  ILP_MODE_SIMPLE this maps to X" semantics.
- Delete `ilp_for/detail/macros_simple.hpp`. Keep `iota.hpp` (public, has its
  own tests).
- Tests: strip the `#if !defined(ILP_MODE_SIMPLE)` guards from the 9
  correctness files (leave `#if defined(...)`-style positive checks like the
  `default_mode` STATIC_REQUIRE alone — flip its expectation branches only if
  needed; it already handles both). Update `test_all_modes.sh` to run
  compile_fail and runtime_fail in **both** legs.
- Docs: README "Debugging" section rewrite (the macro-to-simple-expansion
  table is now wrong — `ILP_MODE_SIMPLE` = `Mode::Simple` everywhere, same
  table as the function API); DESIGN_NOTES item 1 → **Status: resolved**
  (divergence removed by unification); item 2/3 notes mentioning "SIMPLE mode
  legitimately compiles the mismatch" need updating — it no longer does.

---

## Item 4 — `-Wshadow` on nested loops: `#pragma GCC system_header`

### Decision

Not the DESIGN_NOTES option-A pragma push/pop (dead end: `_Pragma` cannot be
placed mid-statement, and whole-statement wrapping would suppress the *user
body's* legitimate shadow warnings). Instead, the textbook mechanism that
keeps stdlib macros `-Wshadow`-clean: mark the **macro-definition region** of
`ilp_for.hpp` as a system header. Tokens spelled in system-header macros are
exempt from `-Wshadow`; the user's body tokens are macro *arguments*, keep
their user-file spelling locations, and stay fully warned.

**Placement is load-bearing:** the pragma goes **after the `#include`s, before
the macro definitions** — files included below a `system_header` pragma become
system headers too, which (verified) would swallow the intentional large-N
deprecation warning from `loops_common.hpp`. After-includes placement
preserves it.

```cpp
// ilp_for.hpp, immediately after the last #include:
#if !defined(ILP_NO_SYSTEM_HEADER) && (defined(__GNUC__) || defined(__clang__))
#pragma GCC system_header
#endif
```

`ILP_NO_SYSTEM_HEADER` is the maintenance escape hatch: **this repo's own test
builds must define it** (add to `tests/CMakeLists.txt` non-MSVC flags) so the
header itself stays warning-visible during development. The new `-Wshadow`
regression probe (below) compiles *without* it.

### Prototype-verified (GCC `-Wshadow`, Clang `-Wshadow-all`)

- Nested 2-level macro TU: shadow warnings drop from ~8–31 to **0** on both
  compilers.
- A user's own shadowing inside the same TU still warns (exactly 1, at the
  user's line).
- The N=128 deprecation warning still fires; poison `static_assert`s
  unaffected (errors are never suppressed).
- Full suite unaffected.

### Tests / docs

- New compile-check probe (fold into the compile_fail harness as a
  `// COMPILE_OK`-with-flags case, `EXTRA_FLAGS: -Wshadow` — extend the driver
  to support EXTRA_FLAGS like runtime_fail's, or add a tiny standalone check in
  `test_all_modes.sh`): nested `ILP_FOR` + a deliberate user shadow, assert
  warning count == 1 via `-Werror=shadow`? Simplest robust form: compile with
  `-Wshadow -Werror=shadow` a TU that has nested loops and NO user shadowing —
  must compile clean. Keep the user-shadow-still-warns assertion as a second
  case compiled expecting failure under `-Werror=shadow`.
- MSVC `/W4` (C4456/57/59) is not covered by this pragma — CI is green there
  today; note as out of scope in DESIGN_NOTES item 4's resolution.
- DESIGN_NOTES item 4 → **Status: resolved** (mechanism + placement rationale +
  the option-A dead-end note so it isn't re-attempted).

---

## Item 5 — Mixed-API nesting UB: debug-mode unconsumed-Proxy detection

### Decision

The nesting itself is undetectable (the macro cannot inspect enclosing scopes),
but the bug's *signature* is: **a loop-result `Proxy` is destroyed without ever
being converted to a value**. Every legitimate flow converts the Proxy (top-level
`return propagate_return(...)` into a declared return type; function-API
`*std::move(r)`). So, under the existing `ILP_TYPECHECK_ENABLED` gate:

- `Proxy` (both `ForResult::Proxy` and `ForResultTyped<R>::Proxy`) gains
  `mutable bool ilp_debug_consumed = false;` and a destructor that calls a new
  `detail::swallowed_proxy_abort()` if unconsumed — message naming the
  mixed-API cause and the fix (nested `for_loop` + `ctrl.return_with`),
  pointing at DESIGN_NOTES item 5.
- Every conversion operator (GCC/Clang `&&`-qualified, both MSVC overloads,
  and the `void operator*() &&` member) sets the flag first.

### Prototype-verified

- The DESIGN_NOTES item-5 repro (macro loop with `ILP_END_RETURN` inside an
  `ilp::for_loop` body, every iteration matching — the previously *silent
  wrong answer* variant) now **aborts with the actionable message**.
- Full 352-assertion suite: **zero false positives** (helper functions called
  from inside function-API bodies are unaffected — their Proxies convert
  normally inside the helper).
- Honest limitation for the docs: on iterations where the nested loop finds
  *no* match, the enclosing lambda still falls off the end of a non-void
  deduced-return lambda — that UB precedes our check and still warns via
  `-Wreturn-type` / traps under UBSan. The Proxy check fires deterministically
  on the **first matching iteration**, which any test exercising the path hits.
  So: detection net, not a semantics fix — item 5 stays "documented + now
  debug-detected" rather than "resolved"; update its Proposed plan section to
  record that "detect the mismatch" was achieved via consumption tracking.

### Implementation notes for Sonnet

- Prototype covered `ForResult::Proxy` only — apply identically to
  `ForResultTyped<R>::Proxy` (it lacks the `operator*` member; just dtor + its
  two/one conversions).
- In debug mode the Proxy gains a user-declared destructor (non-trivial type);
  release (`NDEBUG`) is untouched — **re-run the NDEBUG asm byte-identity check**
  (same probe/flags as TYPECHECK_PLAN) to prove zero cost, and re-run
  `release_layout.cpp`-style guarantees (Proxy isn't in SmallStorage, so layout
  is unaffected; the asm check is the meaningful one).
- Copy semantics: with a user dtor the implicit copy ctor is still generated
  (deprecated-but-legal); our flows never copy a live Proxy (prvalue elision
  end-to-end). If a compiler warns, define the copy ctor explicitly to transfer
  `consumed = true` from the source (treat a copied-from Proxy as consumed by
  its copy).
- New runtime_fail case `swallowed_proxy.cpp`: the item-5 repro, expect
  `// RUN_ABORT: swallowed return value`. Update DESIGN_NOTES item 5 and the
  README nested-loops caveat ("debug builds detect this and abort with a
  message" alongside the existing do-not-do-this text).

---

## Suggested implementation order

1. Item 1 (unification) first — it deletes code the other two would otherwise
   have to keep consistent, and extends the harnesses that items 4/5 add cases
   to.
2. Item 5 (Proxy check) — small, isolated in ctrl.hpp, rides the existing
   typecheck gate and runtime_fail harness.
3. Item 4 (system_header) last — placement interacts with the final include
   order of ilp_for.hpp after item 1's deletions.
4. Full verification matrix after each: GCC+Clang × both modes × both
   harnesses (now both-mode), sanitizer subset, NDEBUG asm identity.
5. Docs last: README Debugging rewrite, DESIGN_NOTES items 1/4 resolved +
   item 5 updated, this file's Status flipped.
