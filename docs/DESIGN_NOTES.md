# Design Notes — Investigation Findings & Proposed Plans

**Status:** Living document. Each section records what was found (with
reproductions), then a proposed plan; items marked **Status: resolved** have since
been implemented (see the linked plan doc in that item). Items without a resolved
status are still open.

These notes cover sharp edges in the macro layer and its function-API/nesting
extensions:

1. Bare `return;` in a loop body silently means different things in the two modes.
   **Resolved** — see the item.
2. The `ILP_END` vs `ILP_END_RETURN` runtime failure mode. **Resolved** — see the
   item.
3. The type-erased SBO recovers the value as the *enclosing function's* return type,
   not the type that was stored — a silent type pun. Also applies to nested
   propagation hops (see the cross-reference within the item). **Resolved** — a
   debug-mode check now aborts on mismatch; the release-mode pun itself is
   unchanged (by design — see the item).
4. `-Wshadow` fires on nested `ILP_FOR` because the expansion locals collide.
   **Resolved** — see the item.
5. A macro loop nested inside a function-API lambda is undefined behavior, not a
   clean discard. **Debug-mode detection added** for the non-UB degenerate case
   (silent discard); the UB itself is unchanged and remains a documented
   API-mixing hazard — see the item.

Environment used for the reproductions below: Linux x86-64, GCC 13.3.0, Clang
(system), `-std=c++20`. The library is header-only; reproductions compiled against
the in-tree `ilp_for.hpp`.

---

## 1. Bare `return;` in a loop body: continue in default mode, function-return in SIMPLE

### Finding

In the default (ILP) build, `ILP_FOR(...) { body } ILP_END;` lowers the body into an
immediately-invoked lambda:

```cpp
[&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrl& ilp_detail_ctrl)
{ /* user body */ }
```

The lambda returns `void`, and the control macros are all bare `return;`:

- `ILP_CONTINUE` → `do { return; } while (0)`
- `ILP_BREAK`    → `do { ilp_detail_ctrl.ok = false; return; } while (0)`
- `ILP_RETURN(x)`→ sets storage, then `return;`

So a user who writes a *bare* `return;` in the body gets **continue semantics** — it
returns from the body lambda and the harness advances to the next iteration.

In `ILP_MODE_SIMPLE`, the same source expands to a plain `for` loop with **no
lambda**. A bare `return;` is now a `return` from the *enclosing function*.

The two builds therefore compile the same source to different behavior, silently,
whenever the enclosing function returns `void` (or when the bare return is reachable
and the function's return type allows a valueless return).

### Reproduction

```cpp
void run(const std::vector<int>& data, int& sum) {
    ILP_FOR(auto i, 0, (int)data.size(), 4) {
        if (data[i] < 0) return;   // user intends "continue"
        sum += data[i];
    } ILP_END;
}
// data = {1, 2, -1, 3, 4}
```

| Build | Result | Meaning of bare `return;` |
|-------|--------|---------------------------|
| default | `sum = 10` | continue (lambda return) — keeps processing after `-1` |
| `-DILP_MODE_SIMPLE` | `sum = 3` | returns from `run()` — stops at `-1` |

(When the enclosing function returns a non-void type, the SIMPLE build instead fails
to compile with `return-statement with no value` — a louder but still mode-dependent
outcome.)

This is the most dangerous of the four issues because it is a **silent correctness
divergence between the two supported build modes**, and `ILP_MODE_SIMPLE` is exactly
the mode people reach for when debugging, i.e. when they most trust the semantics to
match.

### Proposed plan

Goal: make a bare `return;` in a loop body **ill-formed in the default build**, so the
mistake is caught at compile time (in default *and* CI) before it can diverge in
SIMPLE mode. Users must use `ILP_CONTINUE` to express "skip this element."

Mechanism — give the body lambda a non-`void` return type (an empty control-token
type), so a valueless `return;` becomes "return-statement with no value in function
returning `Flow`" → a hard error.

The complication is fall-through: the user's body legitimately ends without any
control macro on the common path, and falling off the end of a non-`void` lambda is
ill-formed/UB. We cannot inject a trailing `return token;` *inside* the user's braces
because the user types both braces today (`ILP_FOR(...) { ... } ILP_END`).

Two candidate restructurings:

- **(A) Macro owns the lambda braces.** `ILP_FOR(...)` ends with `... [&](params) -> ::ilp::detail::Flow {`
  and `ILP_END` begins with `return ::ilp::detail::Flow::fallthrough; }`. The user's
  `{ ... }` then becomes a *nested block* inside the lambda. Bare `return;` → error;
  `ILP_CONTINUE` → `return ::ilp::detail::Flow::next;`; fall-through reaches the
  injected trailing token. Cost: the user's braces become an extra block scope
  (semantically harmless), and every control macro must `return` a token.
- **(B) Sentinel-typed `return`.** Keep `void` ergonomics but redefine the control
  macros to return a token and additionally `static_assert`/concept-check the body —
  rejected: cannot catch a *bare* `return;` the user writes by hand, which is the
  whole point.

Plan favors **(A)**. Token type is an empty struct; `ILP_BREAK`/`ILP_RETURN` set
`ilp_detail_ctrl` exactly as today and then `return` the token. The harness
(`for_loop_*_impl`) keeps calling `body(i, ctrl)` and ignoring the return value, so
control flow is unchanged.

**Asm verification (required by the task).** The token is an empty type returned by
value and the body is `always_inline`, so the change must be a no-op in codegen. Verify
with the standalone godbolt examples, which are self-contained copies:

```bash
# baseline (current tree)
g++ -std=c++20 -O3 -march=native -S godbolt_examples/loop_with_break.cpp  -o /tmp/break_before.s
g++ -std=c++20 -O3 -march=native -S godbolt_examples/loop_with_return.cpp -o /tmp/ret_before.s
# ... apply change, regenerate the godbolt copies per godbolt_examples/INSTRUCTIONS.md ...
g++ -std=c++20 -O3 -march=native -S godbolt_examples/loop_with_break.cpp  -o /tmp/break_after.s
diff <(grep -E '^\s+[a-z]' /tmp/break_before.s) <(grep -E '^\s+[a-z]' /tmp/break_after.s)
```

Acceptance: the instruction stream of the user-facing hot functions
(`process_until_negative_ilp`, the return examples) is byte-identical modulo label
numbering, on both GCC and Clang at `-O3 -march=native`. Repeat for
`loop_with_return.cpp` and `loop_with_return_typed.cpp`.

**Migration / breaking-change note.** Any existing user code that relied on bare
`return;` as continue stops compiling in the default build (intended). The fix is
mechanical (`return;` → `ILP_CONTINUE;`) and should be called out in the changelog.
SIMPLE mode cannot ban bare `return;` (it is a literal `for`), but because the default
build now rejects it, a project that builds both ways is protected by its default/CI
build.

**Status: resolved — by unifying the modes, not by the (A) restructuring above.**
Implemented per [OPEN_ITEMS_PLAN.md](OPEN_ITEMS_PLAN.md): rather than giving the body
lambda a non-`void` return type, `ILP_MODE_SIMPLE`'s separate macro implementation
(`ilp_for/detail/macros_simple.hpp`, a literal `for`/`break`/`continue`/`return`
lowering) was deleted outright. `ILP_MODE_SIMPLE` now only flips `ilp::default_mode`
to `Mode::Simple` (the runtime unrolling strategy — remainder-loop-only, one bounds
check per iteration), exactly as it already did for the function API; the macro
*expansion* is unconditional and identical in both modes. Since every `ILP_FOR` body
is always a lambda now, bare `return;` means *continue* — the same thing it already
meant in the default build — in both modes, with no divergence left to catch. This
also means the (A)/(B) approaches above were not needed: there's no more separate
literal-`for` expansion for a bare `return;` to diverge against.

Consequences of the unification, beyond closing this item:
- The `ILP_END`/`ILP_END_RETURN` compile-time mismatch enforcement (item 2) and the
  debug-mode SBO type check (item 3) now apply under `ILP_MODE_SIMPLE` too — both were
  previously default-build-only because SIMPLE mode's separate expansion didn't go
  through the same machinery. `tests/test_all_modes.sh` now runs
  `tests/compile_fail`/`tests/runtime_fail` once per mode instead of default-mode-only.
- **Breaking changes**, all closing divergence rather than introducing new
  restriction: (1) bare `return;` in a loop body now means *continue* under
  `ILP_MODE_SIMPLE` too (previously: returned from the enclosing function) — anyone
  relying on the old behavior was already writing mode-divergent code; (2)
  `ILP_MODE_SIMPLE` no longer lowers to a literal `for` loop, so single-stepping goes
  through one lambda frame, same as the function API's `Mode::Simple` (see the
  README's [Debugging](../README.md#debugging) section); (3) bodies using raw
  `break;`/`continue;`/`goto` out of the loop no longer compile under
  `ILP_MODE_SIMPLE` (they never compiled in the default build — this removes a
  mode-only allowance, not a previously-working pattern in the default build).
- `ilp_for/detail/macros_simple.hpp` is deleted; `ilp_for/detail/iota.hpp` (which it
  had included) is kept — it's a public header with its own tests
  (`tests/correctness/test_iota.cpp`).

---

## 2. `ILP_END` vs `ILP_END_RETURN` — runtime failure mode

### Finding

`ilp_end_with_return_error()` (in `ilp_for/detail/ctrl.hpp`) **is** `[[noreturn]]` and
**is** loud: it writes a multi-line diagnostic to `stderr` and calls `std::abort()`.

```cpp
[[noreturn]] inline void ilp_end_with_return_error() {
    std::fprintf(stderr, "\n*** ILP_FOR ERROR ***\n"
                         "ILP_RETURN was called but ILP_END was used instead of ILP_END_RETURN.\n"
                         "The return value would be silently discarded. This is a bug.\n"
                         "Fix: Change ILP_END to ILP_END_RETURN in the enclosing function.\n\n");
    std::abort();
}
```

`ILP_END` expands to a conditional that triggers it when a return was set but the
non-return terminator was used:

```cpp
ilp_detail_ret.has_return ? (::ilp::detail::ilp_end_with_return_error(), false) : false
```

### Reproduction

Using `ILP_RETURN(i)` in a body but closing with `ILP_END` (instead of
`ILP_END_RETURN`), then running with a target that is actually found:

```
*** ILP_FOR ERROR ***
ILP_RETURN was called but ILP_END was used instead of ILP_END_RETURN.
The return value would be silently discarded. This is a bug.
Fix: Change ILP_END to ILP_END_RETURN in the enclosing function.

Aborted        # exit 134 (SIGABRT)
```

### Documentation of the failure mode (this is the deliverable for #2)

- **Type of failure:** *runtime*, not compile time. The mismatched terminator
  compiles cleanly; the abort only happens if an `ILP_RETURN` is *actually executed*
  at runtime (i.e. `has_return == true`).
- **Latent-bug caveat:** if the `ILP_RETURN` branch is never taken on a given run
  (target not found, predicate never true), `has_return` stays `false`, `ILP_END`
  passes, and the mismatch is **invisible** for that run. The guard catches the bug
  only on inputs that trigger the return. Test data must exercise the return path.
- **Partial compile-time backstop:** `ForResult` / `ForResultTyped` are
  `[[nodiscard("ILP_RETURN value ignored - did you mean ILP_END_RETURN?")]]`, which
  catches *some* misuse shapes at compile time, but not the `ILP_END`-swallows-the-
  result case (the result is consumed by the ternary, so `nodiscard` is satisfied).
- **Behavior on trigger:** loud `stderr` message + `std::abort()` → `SIGABRT`,
  exit 134. `[[noreturn]]` is correct, so the compiler knows the `true` branch does
  not return and optimizes accordingly.

Proposed action for #2 is documentation only (no code change needed): add the above
"runtime, and latent unless the return path is exercised" caveat to the README's
API/Important-Notes section next to the existing `ILP_END_RETURN` guidance, so users
know the guard is necessary-but-not-sufficient and that tests must hit the return
branch. (Optionally, item #3's compile-time type check would also let us strengthen
this into a compile-time error in a follow-up; tracked there.)

**Status: resolved.** Implemented as a compile-time error instead of the
documentation-only mitigation above — see
[END_ENFORCEMENT_PLAN.md](END_ENFORCEMENT_PLAN.md). The opening and closing macros
jointly form one call to a `detail::macro_for*` entry point; the closing macro appends
a tag (`end_tag_t` for `ILP_END`, `end_return_tag_t` for `ILP_END_RETURN`) that
selects which ctrl type the body lambda is instantiated against. `ILP_END` selects a
break-only `EachCtrl` whose `return_with` is a poisoned `static_assert`, so a body
using `ILP_RETURN` but closed with `ILP_END` now fails to compile with a message
naming the fix. The runtime `abort()` path (`ilp_end_with_return_error`) has been
deleted. This also introduced `ilp::for_each`/`for_each_range` (break/continue-only,
`void`-returning) as the function-API counterpart to `EachCtrl`.

---

## 3. Type-erased SBO recovers the *function's* return type, not the stored type — silent pun

### Finding

`ILP_RETURN(x)` stores into the SBO via `SmallStorage::set`:

```cpp
template<typename T> void set(T&& val) {
    using U = std::decay_t<T>;
    static_assert(sizeof(U)  <= arch::sbo_size, ...);   // checks the STORED type
    static_assert(alignof(U) <= arch::sbo_size, ...);
    static_assert(std::is_trivially_destructible_v<U>, ...);
    new (buffer) U(static_cast<T&&>(val));              // placement-new of U
}
```

Recovery at `ILP_END_RETURN` goes through `ForResult::operator*` → `Proxy`, whose
conversion operator is templated on the *target* type `R`:

```cpp
template<typename R> requires(!detail::is_optional_v<R>)
operator R() && { return s.template extract<R>(); }   // R deduced from RETURN context

template<typename R> R extract() {
    return static_cast<R&&>(*std::launder(reinterpret_cast<R*>(buffer)));
}
```

`R` is deduced from the **enclosing function's return type** (the macro does
`return *std::move(ilp_detail_ret);`), *not* from the type that `set()` stored. There
is no check that the recovered type matches the stored type. If a function returns
`long` but the body does `ILP_RETURN(i)` with `int i`, the buffer holds a 4-byte `int`
and is then read as an 8-byte `long`. The bytes above the stored `int` were never
initialized.

This affects the **untyped** path (`ILP_FOR` / `ILP_FOR_AUTO`, `SmallStorage`). The
**typed** path (`ILP_FOR_T*`, `TypedStorage<R>`) is not type-erased — `R` is fixed by
the macro argument — so it is not exposed to this pun (a wrong `ILP_RETURN` type there
fails to convert/compile instead).

### Reproduction

```cpp
long find_it(const std::vector<int>& data, int target) {
    ILP_FOR(auto i, 0, (int)data.size(), 4) {
        if (data[i] == target) ILP_RETURN(i);   // stores int, function returns long
    } ILP_END_RETURN;
    return -1;
}
```

| Build | Result for found index 3 |
|-------|--------------------------|
| `-O0` | `result = 94124208291843` (`0x559b00000003`) — low 4 bytes = stored `3`, high 4 bytes = uninitialized stack |
| `-O2` | `result = 3` — optimizer happened to forward the value; **not guaranteed** |
| `-O1 -fsanitize=undefined` | `result = 3` — UBSan did **not** flag the indeterminate read |

So the mistake is a genuine silent pun: wrong at `-O0`, accidentally "right" at `-O2`,
and not caught by UBSan. Because `sbo_size == sizeof(intmax_t) == 8`, reading a `long`
stays inside the buffer (no overflow), so it is a wrong-*value* bug, not a crash.

### Proposed plan

Add a **debug-mode stored-vs-recovered type check** so the pun is caught.

- Record what was stored. `SmallStorage::set<U>()` records a lightweight type id —
  either `&typeid(U)` or a `constexpr` per-type tag pointer
  (`template<class U> constexpr char type_tag; auto id = &type_tag<U>;`, which works
  without RTTI) plus `sizeof(U)`/`alignof(U)` — into a debug-only member of `ForCtrl`
  / `SmallStorage` (guarded by `#ifndef NDEBUG` or a dedicated `ILP_DEBUG_TYPECHECK`
  so release builds keep the SBO at exactly `sbo_size` with zero overhead).
- Check on extract. `SmallStorage::extract<R>()` asserts the recorded tag equals the
  tag for `R` (or at least `sizeof(R) == stored_size && alignof(R) == stored_align`)
  before the `launder`/read, with a message naming both types:
  `"ILP_RETURN stored T but enclosing function returns R"`.
- Prefer a **compile-time** check where possible. The recovered `R` is known at the
  `ILP_END_RETURN` site, but the stored `U` is only known inside the body lambda.
  A full compile-time match would require threading the stored type into `ForResult`'s
  type (e.g. making the body publish its `ILP_RETURN` type via the concept/return
  channel). That is a larger change; the runtime debug assert is the pragmatic first
  step and is sufficient to convert the silent pun into a loud failure under tests.
- Keep release zero-cost. All of the above lives behind the debug guard; release
  `SmallStorage` stays `alignas(sbo_size) char buffer[sbo_size]` with no extra
  members, so #1/#2 asm verification and existing benchmarks are unaffected.

This also gives #2 a path to a stronger guarantee later: once the stored type is known
to the result object, the `ILP_END` vs `ILP_END_RETURN` mismatch can become a
compile-time error instead of a runtime abort.

**Extends to nested propagation.** [NESTED_RETURN_PLAN.md](NESTED_RETURN_PLAN.md)'s
`detail::propagate_return` carries a value from an inner loop's ctrl into an outer
loop's ctrl at each nesting level, and it reuses exactly this recovery path: an
untyped inner result is read back via `extract<R2>()` where `R2` is the *outer*
loop's type (or, at the outermost level, the enclosing function's declared return
type via the `Proxy`). The stored-vs-recovered type is never checked at any hop, so
the pun described above can now occur *between nesting levels*, not just between the
top-level `ILP_RETURN` and the function's return type — e.g. an untyped
`ILP_RETURN(some_int)` propagating out through an `ILP_FOR_T(long, ...)` outer loop
reinterprets the 4 stored bytes as an 8-byte `long`. The proposed debug-mode type
check above should cover `propagate_return`'s extracts, not only the top-level
`ForResult`/`ForResultTyped` recovery, when implemented. Documented as a caveat in
the README's [Nested Loops](../README.md#nested-loops) section and in
`tests/correctness/test_nested_return.cpp` in the meantime.

**Status: resolved (debug-mode check).** Implemented per
[TYPECHECK_PLAN.md](TYPECHECK_PLAN.md): `SmallStorage::set` records an RTTI-free
type tag (the address of a per-type `inline constexpr` variable — unique
program-wide, so cross-TU comparison is exact) and `SmallStorage::extract<R>`
aborts with a message naming both types if `R` doesn't match what was stored.
Gated assert-style (`!NDEBUG`, with `ILP_DEBUG_TYPECHECK`/`ILP_NO_DEBUG_TYPECHECK`
overrides), so the release-mode contract described above is **unchanged** — types
must still match exactly; the check only converts the silent wrong-value bug into
a loud debug-build abort. Because every recovery in the library — the top-level
`Proxy` conversion *and* every `propagate_return` hop across nested loops — funnels
through this one `extract<R>`, the nested-propagation extension called for above is
covered automatically, with no changes to `propagate_return` itself. Verified
zero-cost under `-DNDEBUG`: generated assembly is byte-identical to the pre-check
headers on GCC and Clang, and `sizeof(SmallStorage)` is unchanged (see
`tests/runtime_fail/release_layout.cpp`, a permanent CI regression test for this).
The `tests/runtime_fail/` harness covers the top-level pun, the same-size pun
(`int`→`float`, proving the check is type-identity-based rather than
size-based), and the nested-hop pun from the paragraph above.

---

## 4. `-Wshadow` on nested `ILP_FOR`

### Finding

The expansion locals (`ilp_detail_ret`, `ilp_detail_ctx`) and the body-lambda
parameter (`ilp_detail_ctrl`) have **fixed names**. Nesting `ILP_FOR` inside another
`ILP_FOR` puts the inner expansion's identically-named entities inside the outer body
lambda, so each inner level *shadows* the outer's. This shadowing is in fact what
makes nested control correct — an inner `ILP_BREAK` must target the inner
`ilp_detail_ctrl` — but it trips `-Wshadow`.

The project's own test suite compiles with `-Wall -Wextra` only (see
`tests/CMakeLists.txt`), so it is clean today. The problem surfaces in *user* projects
that enable `-Wshadow`.

### Reproduction

Two-level nested `ILP_FOR`, warning counts on `-c`:

| Compiler / flag | Warnings |
|-----------------|----------|
| GCC `-Wshadow` | 4 (shadows of `ilp_detail_ret`, `ilp_detail_ctx`, `ilp_detail_ctrl`, …) |
| Clang `-Wshadow` | 0 (Clang's plain `-Wshadow` ignores this category) |
| Clang `-Wshadow-all` | 3 |
| GCC `-Wall -Wextra` (no `-Wshadow`) | 0 |

GCC sample:

```
./ilp_for.hpp:53:31: warning: declaration of 'ilp_detail_ret' shadows a previous local [-Wshadow]
   53 |     if ([[maybe_unused]] auto ilp_detail_ret = [&]() -> ::ilp::ForResult { \
./ilp_for.hpp:56:82: warning: declaration of 'ilp::ForCtrl& ilp_detail_ctrl' shadows a parameter [-Wshadow]
```

So: harmless but noisy for `-Wshadow`/`-Wshadow-all` users, and the noise scales with
nesting depth.

### Proposed plan

Options, in rough order of preference:

- **(A) Localize warning suppression in the macros.** Wrap the expansion in
  `_Pragma`-based diagnostic push/ignore so the generated locals don't warn, without
  touching user diagnostics:
  ```c
  _Pragma("GCC diagnostic push")
  _Pragma("GCC diagnostic ignored \"-Wshadow\"")
  /* ... expansion ... */
  _Pragma("GCC diagnostic pop")
  ```
  Works for GCC and Clang (`GCC diagnostic` is accepted by Clang). Risk: `_Pragma`
  inside an expression-context macro (`ILP_FOR` expands mid-expression, inside an
  `if` init) is constrained — pragmas are not allowed in arbitrary expression
  positions. Feasibility of placing push/pop around the `if (...)` needs prototyping;
  may only cover part of the expansion. Must re-run #1's asm check to confirm pragmas
  don't perturb codegen (they should not).
- **(B) Counter-based unique names.** Suffix the expansion locals with `__COUNTER__`
  (or `__LINE__`) so nested levels get distinct identifiers and never shadow:
  `ilp_detail_ret_42`, etc. Pro: removes the warning at the root cause for all
  compilers/flags, no pragma portability questions. Con: `ILP_BREAK` / `ILP_RETURN`
  reference `ilp_detail_ctrl` by name from the *user's* body, so the token-paste must
  be coordinated between the opening macro and the control macros. Because the control
  macros expand in a different invocation than the opening macro, they cannot see the
  opener's `__COUNTER__` value. This makes (B) hard without a per-scope indirection
  (e.g. a fixed-name reference alias to the unique object) — which itself reintroduces
  a fixed name. Likely not worth the complexity.
- **(C) Document only.** State that nested `ILP_FOR` under `-Wshadow`/`-Wshadow-all`
  produces benign shadow warnings, and suggest users localize the suppression around
  nested loops. Lowest effort; leaves the noise.

Recommendation: prototype **(A)**; if pragmas can be placed cleanly around the `if`
init-statement without breaking the for-loop syntax or codegen, ship it. If not, fall
back to **(C)** for now and keep **(B)** out of scope. Whatever is chosen, add a
nested-loop translation unit compiled with `-Wshadow` (GCC) and `-Wshadow-all`
(Clang) to the test matrix so regressions are caught.

**Status: resolved — via `#pragma GCC system_header`, not the (A) push/pop
sketched above.** `_Pragma` push/pop turned out to be a dead end: it cannot be
placed mid-statement (`ILP_FOR` expands inside an `if` init-statement), and
whole-statement wrapping would suppress the *user body's* own legitimate shadow
warnings along with the macro-introduced ones. Implemented per
[OPEN_ITEMS_PLAN.md](OPEN_ITEMS_PLAN.md) instead: `ilp_for.hpp` marks the region
**after its `#include`s and before the macro definitions** with
`#pragma GCC system_header` (GCC/Clang only; gated behind
`ILP_NO_SYSTEM_HEADER`, which this repo's own test build defines via
`tests/CMakeLists.txt` so the header stays warning-visible during development).
Tokens spelled in a system-header macro are exempt from `-Wshadow`; the user's
body tokens are macro *arguments* and keep their user-file spelling locations, so
real shadowing in user code is still fully warned — verified by
`tests/compile_fail/wshadow_nested_ok.cpp` (nested `ILP_FOR`, no user shadowing,
compiles clean under `-Werror=shadow`) and
`tests/compile_fail/wshadow_user_still_warns.cpp` (deliberate user shadowing,
still fails to compile under the same flags). Placement matters: the pragma marks
everything *after* it as a system header, including subsequent `#include`s, so it
must come after this header's own `#include`s — otherwise `loops_common.hpp`'s
intentional large-N deprecation warning would be swallowed too (verified). MSVC
`/W4` (C4456/57/59) is not covered by this pragma (`#pragma GCC system_header` is
a GCC/Clang extension); CI is green there today, so this is out of scope rather
than a gap.

**Side effect on clang-tidy (found and fixed on this branch).** Marking the macro
definitions as a system header also makes `clang-tidy` treat macro-expanded call
sites in *user* code as non-user/system code by default, which silently drops the
`ilp-loop-analysis` custom check's diagnostics there too (`tools/clang-tidy/`) —
not just `-Wshadow`. Fixed by passing `-header-filter='.*' -system-headers` to
every `clang-tidy` invocation against this codebase (the CI `clang-tidy-check`
job, `tools/clang-tidy/test/test_clang_tidy.cpp`'s test harness, the VS Code
tasks, and the README usage examples were all updated). Anyone running
`clang-tidy` against code that uses `ilp_for.hpp` needs those same two flags for
their own checks to fire on ILP loop call sites.

---

## 5. A macro loop nested inside a function-API lambda: UB, not a clean swallow

### Finding

[NESTED_RETURN_PLAN.md](NESTED_RETURN_PLAN.md)'s propagation mechanism works by
having `ILP_END_RETURN` look up the unqualified name `ilp_detail_ctrl` at its own
expansion point: nested inside another *macro* loop's body, that name resolves to
the outer loop's ctrl parameter (also always spelled `ilp_detail_ctrl`); at true
top level, it falls back to a global sentinel function of the same name.

This breaks down when the outer scope is a **function-API** lambda instead of a
macro-expanded one. `ilp::for_loop`/`ilp::for_each` bodies name their ctrl parameter
whatever the caller wrote (commonly `ctrl`, not `ilp_detail_ctrl`), so a macro loop
nested inside one of those bodies finds no matching name in scope and falls back to
the same global sentinel a true top-level loop would use. `ILP_END_RETURN`'s
`return ::ilp::detail::propagate_return(ilp_detail_ret, ilp_detail_ctrl);` is a bare
statement embedded directly in whatever scope contains the macro invocation - here,
the **outer function-API lambda's body itself** (not the macro loop's own IIFE,
which has already returned by this point). That single `return` statement, on the
branch where the inner macro loop found a match, makes the *outer lambda's* return
type get deduced as non-void (a `Proxy`). But the outer lambda's other control-flow
paths - every iteration where the inner macro loop's search comes up empty - fall
through to the end of the lambda body with **no return statement at all**.

**This is not a benign discard - it's undefined behavior** (a value-returning
function/lambda reaching its end without executing a `return`), confirmed with
`-fsanitize=undefined`: `runtime error: execution reached the end of a
value-returning function without returning a value`, which traps (`SIGILL`) under
UBSan and is free to do anything in a non-sanitized build. The "silently returns
not-found" behavior only happens in the degenerate case where the inner macro loop's
condition happens to match on *every single* outer iteration (so every path through
the outer lambda does hit the injected `return`) - in that specific case, no path
falls off the end, there is no UB, and the propagated value genuinely is discarded
cleanly (the outer `for_loop`'s ctrl was never told about a match, so the outer call
reports not-found). Any realistic search - where the inner loop sometimes finds
nothing - hits the UB path.

### Reproduction

```cpp
int find_first_match(const std::vector<std::vector<int>>& rows, int target) {
    auto outer = ilp::for_loop<2>(std::size_t{0}, rows.size(), [&](auto r, auto& outer_ctrl) {
        ILP_FOR(auto c, std::size_t{0}, rows[r].size(), 4) {
            if (rows[r][c] == target)
                ILP_RETURN(static_cast<int>(r * 100 + c));
        }
        ILP_END_RETURN;   // injects a `return` into the OUTER lambda's body; any
                           // outer iteration whose inner search finds nothing falls
                           // off the end of that (now non-void) lambda instead
        // outer_ctrl.return_with(...) was never called, so `outer` reports not-found
        // on the paths that don't crash first
    });
    if (outer) return *std::move(outer);
    return -1;
}
// rows = {{1,2,3},{4,5,6},{7,8,9}}, target = 6: the r=0 iteration's inner search
// finds nothing before r=1 would find the match, so control falls off the end of
// the outer lambda on r=0 -> UB, reproduced as a UBSan trap (SIGILL).
```

**Severity:** narrow in the sense that it's self-inflicted - it requires deliberately
mixing the two APIs (a loop macro inside a function-API lambda body) rather than
nesting within one API consistently - but the actual failure mode (UB/crash on any
realistic input, not a quietly-wrong value) is worse than the README's current
"silently swallowed" wording suggests. The README's
[Nested Loops](../README.md#nested-loops) section already tells users not to do this
and names the correct alternative (nested `for_loop` calls, propagating explicitly
via `ctrl.return_with(...)`); its wording should be tightened to say "undefined
behavior" rather than "silently swallowed" given the above.

### Proposed plan

Not fixed; documented here as the counterpart to items 1-4 so it's tracked
alongside them rather than only living in a README caveat. Two possible directions,
neither attempted:

- **Detect the mismatch.** If the function-API ctrl types (`ForCtrl`, `EachCtrl`,
  etc.) exposed a distinguishing trait, a macro loop could `static_assert` that its
  own top-level fallback path is only reached from genuine top-level scope - but the
  macro has no way to inspect what identifiers exist in an *enclosing* scope beyond
  the unqualified lookup it already does, so detecting "I'm nested inside some
  lambda, but not one of mine" doesn't have an obvious mechanism.
- **Document only (current state).** Keep this as a documented caveat and rely on
  users not mixing macro and function-API loops within a single nesting chain -
  which is also the simpler mental model regardless (pick one API per call chain).
  Given that the actual failure mode is UB rather than a clean wrong-value swallow,
  this option is weaker than it would be for a merely-wrong-value bug - a user who
  ignores the caveat doesn't just get a wrong answer, they get a program that may
  crash unpredictably depending on iteration order and optimization level.

Given the self-inflicted (mixing two APIs) nature of the trigger and the lack of an
obvious detection mechanism, documentation is likely the right near-term answer, not
a code fix - but the severity re-assessment above (UB, not a clean swallow) makes
"detect the mismatch" worth a real attempt in a future pass rather than being ruled
out permanently. Recorded as a numbered item (rather than folded into the
README-only caveat) so a future contributor evaluating the nested-propagation design
sees this gap listed alongside the others.

**Status: detection net added (not a semantics fix) — "detect the mismatch" above
was reattempted and partially succeeds.** Implemented per
[OPEN_ITEMS_PLAN.md](OPEN_ITEMS_PLAN.md): detecting the *nesting* is still
impossible (unchanged from the analysis above — the macro genuinely cannot inspect
what identifiers exist in an enclosing scope), but the bug's *signature* is
detectable: a loop-result `Proxy` (`ForResult::Proxy`, `ForResultTyped<R>::Proxy`)
destroyed without ever being converted to a value. Every legitimate flow converts
its Proxy (top level: into the declared return type; function API: via
`*std::move(r)`); the mixed-API bug is the one path that doesn't. Under the
existing `ILP_TYPECHECK_ENABLED` debug gate, `Proxy` now carries a
`ilp_debug_consumed` flag (set by every conversion operator) and a destructor that
aborts naming the mixed-API cause and the fix if the flag was never set —
verified against the repro above (`tests/runtime_fail/swallowed_proxy.cpp`,
`// RUN_ABORT: swallowed return value`) and zero false positives across the full
test suite. Zero-cost in release (`NDEBUG`): verified byte-identical assembly
(same probe/flags as item 3's check).

**What this does and doesn't cover.** The check fires deterministically on the
first outer iteration whose inner search finds a match — any test that exercises
the return path hits it. But per the severity re-assessment above, the *far more
common* failure mode is the outer lambda falling off its end on an iteration where
the inner search finds *nothing* — that is undefined behavior (confirmed via
UBSan trap), happens **before** the Proxy destructor would ever run on that path,
and this check does nothing for it; it remains exactly as documented above. So
this item stays "documented + now debug-detected for the non-UB degenerate case,"
not "resolved" in the sense of items 1-4 — the underlying UB is unchanged by
design (there is still no known mechanism to detect the nesting itself). Users
must still avoid mixing the two APIs; see the
[README's nested-loops caveat](../README.md#nested-loops) for the recommended
alternative (nested `ilp::for_loop` calls, propagating explicitly via
`ctrl.return_with(...)`).

---

## Incidental finding (not in scope, noted for tracking)

The standalone `godbolt_examples/*.cpp` still use the C++23 `0uz` literal (e.g.
`loop_with_break.cpp:175`), which warns under `-std=c++20`
(`use of C++23 'size_t' integer constant`). The in-repo `README.md` was already
migrated off `0uz`; the godbolt copies were not. These files are deliberately
line-for-line copies of library source per `godbolt_examples/INSTRUCTIONS.md`, so any
fix should go through that regeneration process. Out of scope for this investigation;
listed so it isn't lost.

## Incidental finding: `ilp-loop-analysis` clang-tidy check was broken on `main` — found and fixed

Found while re-verifying the clang-tidy module during the branch review that
followed the END-enforcement and nested-return work. Two separate issues, both
fixed on this branch (`tools/clang-tidy/ILPLoopCheck.cpp`):

- **Regression from this branch:** the check's AST matcher keyed on callee names
  matching `for_loop.*`, which matched the macro layer's expansion before the
  END-enforcement rework. After that rework, `ILP_FOR`/`ILP_FOR_T` expand to
  `::ilp::detail::macro_for*` instead, so the matcher stopped firing on any
  macro-written loop — silently, since nothing in the local test suite builds the
  clang-tidy module (it requires `llvm-18-dev`/`libclang-18-dev`, not part of the
  default dev setup). CI's clang-tidy job greps the check's output for pattern
  names and would have gone red.
- **Pre-existing, unrelated to this branch:** the check's "already-fixed code is not
  re-detected" idempotency test (`ILP_FOR_AUTO` should be skipped, since it already
  selects `N` via `LoopType`) relied on `check()`-time source-text extraction
  (`extractMacroArgs`) to recognize the `_AUTO` macro name and suppress the
  diagnostic. That extraction never actually worked for the `_AUTO` variants — the
  unit test asserting it was not wired into CI (only `test/loop_patterns.cpp`'s
  pattern-detection is), so the rot went unnoticed on `main` as well as on this
  branch.

Both are fixed together by moving the `_AUTO` exclusion to the matcher itself
(`unless(matchesName("_auto"))` on the callee, since `macro_for_auto`/
`macro_for_range_auto` are now distinct, separately-named entry points from the
non-auto macros - see `loops_ilp.hpp`), rather than relying on fragile
check()-time text parsing. Verified against llvm-18: all 8 CI grep patterns
detected on `test/loop_patterns.cpp`, and all 10 of the tool's unit tests pass,
from a reproduced red baseline for the regression.

## Incidental finding: nested `ILP_RETURN` does not propagate through an outer loop's body

Found while adding coverage for the END-enforcement work
([END_ENFORCEMENT_PLAN.md](END_ENFORCEMENT_PLAN.md)). An `ILP_FOR`/`ILP_END_RETURN`
block nested inside *another* `ILP_FOR`'s body does not return its value out of the
true enclosing C++ function - reproduces identically against the pre-enforcement
library snapshot (this is not something the tag-dispatch mechanism introduced or
could have fixed).

```cpp
int find_first_match(const std::vector<std::vector<int>>& rows, int target) {
    ILP_FOR(auto r, std::size_t{0}, rows.size(), 2) {
        const auto& row = rows[r];
        ILP_FOR(auto c, std::size_t{0}, row.size(), 4) {
            if (row[c] == target)
                ILP_RETURN(static_cast<int>(r * 100 + c));
        }
        ILP_END_RETURN;
    }
    ILP_END;
    return -1;
}
// find_first_match({{1,2,3},{4,5,6},{7,8,9}}, 6) returns -1, not 105.
```

**Cause:** `ILP_END_RETURN`'s `return *std::move(ilp_detail_ret);` is a bare C++
`return` statement, textually embedded at whatever scope directly contains the macro
invocation. When the inner `ILP_FOR`/`ILP_END_RETURN` block is nested inside another
`ILP_FOR`'s `{ body }`, that scope is the *outer* loop's body lambda (`[&](auto r,
auto& ctrl){ ... }`) - not the real enclosing function. The `return` only escapes as
far as that outer lambda. Because the outer lambda's return type is deduced (`auto`,
from its single `return` statement) rather than declared, this doesn't produce a
compile error: it silently deduces a return type and discards the value the inner
loop found, falling through to the outer loop's own `return -1;`. GCC does emit a
`-Wreturn-type` "control reaches end of non-void function" warning, but it is easy to
miss among the deprecation/nodiscard warnings this library already emits by design.

**Severity:** narrow - only affects code that nests an `ILP_RETURN`-using loop inside
another loop's body and expects the inner return to escape both loops. The far more
common pattern (an inner *break-only* `ILP_END` loop nested inside an outer
`ILP_END_RETURN` loop, or vice versa with the return used only at the outer level) is
unaffected and is covered by `tests/correctness/test_end_enforcement.cpp`.

**Out of scope for this investigation.** A fix would need the outer loop's body
lambda to have a declared (not deduced) return type matching whatever the innermost
nested `ILP_RETURN` produces, which conflicts with the outer loop potentially having
no `ILP_RETURN` of its own (i.e., being `void`) - this needs its own design pass, not
a fold-in to the END-enforcement work. Listed so it isn't lost.

**Status: resolved.** Implemented per
[NESTED_RETURN_PLAN.md](NESTED_RETURN_PLAN.md), which took the different approach
this note ruled out above: rather than giving the outer body lambda a declared
return type, `ILP_END_RETURN` now dispatches on what unqualified lookup finds for
`ilp_detail_ctrl` at its own expansion point. Nested inside another loop's body,
that name resolves to the *outer* loop's ctrl (shadowing a global sentinel function
that only plain function-scope invocations see), so the inner value is carried into
the outer ctrl and the outer loop's own `ILP_END_RETURN` propagates it one level
further - keeping every lambda's return type `void` or the Proxy, exactly as
before, with no declared-return-type conflict. An enclosing loop closed with plain
`ILP_END` (whose ctrl is `EachCtrl`, break-only) now produces a compile error
naming the fix, extending the END-enforcement mechanism transitively through
nesting. `ILP_RETURN` now means the same thing - return from the enclosing
function - at any nesting depth, matching `ILP_MODE_SIMPLE`.

---

## Suggested sequencing (historical)

This section originally sequenced items #1-#4 by cost/value before any of them were
implemented. Superseded by events: #2 and #3 both shipped (via different mechanisms
than sequenced here — #2 as a compile-time tag-dispatch redesign rather than leaning
on #3's type check; #3 as a debug-mode runtime check rather than #2's originally
imagined side effect of it), and #5 was discovered afterward. All five items have
since been addressed, per [OPEN_ITEMS_PLAN.md](OPEN_ITEMS_PLAN.md) for #1/#4/#5.
Current state:

- **#1** (bare `return;` meaning continue) — resolved, by unifying the two build
  modes (`ILP_MODE_SIMPLE`'s separate macro expansion deleted; it now only flips
  `ilp::default_mode`) rather than the breaking macro-expansion change originally
  sketched below.
- **#2** (`ILP_END`/`ILP_END_RETURN` mismatch) — resolved, compile-time error.
- **#3** (SBO type pun) — resolved, debug-mode abort; release-mode pun unchanged
  by design.
- **#4** (`-Wshadow` on nested loops) — resolved, `#pragma GCC system_header`
  (with a clang-tidy invocation caveat — see the item).
- **#5** (macro nested in function-API lambda is UB) — debug-mode detection net
  added for the non-UB degenerate case; the UB itself is unchanged, no known
  detection mechanism for the general case.

Because #1 shipped via mode unification instead of the originally-sketched
non-`void`-lambda restructuring, it did not need the breaking macro-expansion
change the note below anticipated — no public macro names or expansions changed,
only what a bare `return;` means under `ILP_MODE_SIMPLE` (see item 1's resolution
above for the concrete breaking-change list).
