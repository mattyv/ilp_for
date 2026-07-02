# Design Notes — Investigation Findings & Proposed Plans

**Status:** Investigation only. No code has been changed. Each section records what
was found (with reproductions), then a proposed plan. Nothing here is implemented
yet.

These notes cover four sharp edges in the non-`ILP_MODE_SIMPLE` macro layer:

1. Bare `return;` in a loop body silently means different things in the two modes.
2. The `ILP_END` vs `ILP_END_RETURN` runtime failure mode.
3. The type-erased SBO recovers the value as the *enclosing function's* return type,
   not the type that was stored — a silent type pun.
4. `-Wshadow` fires on nested `ILP_FOR` because the expansion locals collide.

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

---

## Incidental finding (not in scope, noted for tracking)

The standalone `godbolt_examples/*.cpp` still use the C++23 `0uz` literal (e.g.
`loop_with_break.cpp:175`), which warns under `-std=c++20`
(`use of C++23 'size_t' integer constant`). The in-repo `README.md` was already
migrated off `0uz`; the godbolt copies were not. These files are deliberately
line-for-line copies of library source per `godbolt_examples/INSTRUCTIONS.md`, so any
fix should go through that regeneration process. Out of scope for this investigation;
listed so it isn't lost.

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

## Suggested sequencing

1. **#3 debug type check** and **#2 doc caveat** first — smallest, highest-value, no
   public-API or codegen impact (#3 is behind a debug guard).
2. **#1 control-token** next — the biggest correctness win, but a breaking change and
   the one that *requires* the godbolt asm verification before merge.
3. **#4 `-Wshadow`** last — quality-of-life; prototype the pragma approach, else
   document.

None of these change public macro *names*. #1 changes public macro *expansion* and is
a breaking source change for code relying on bare `return;`; it should land behind a
clear changelog entry (and, if desired, a transition `ILP_ALLOW_BARE_RETURN` escape
hatch).
