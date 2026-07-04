# Debug-Mode SBO Type Check — Implementation Plan

**Status:** Implemented (see the `ILP_TYPECHECK_ENABLED` gate, `TypeTag`/`type_tag_v`/
`type_mismatch_abort` and the `SmallStorage` changes in `ilp_for/detail/ctrl.hpp`,
`tests/runtime_fail/`, and the README's "Large Return Types" / "Nested Loops"
updates). Mechanism prototype-validated, then re-verified against the final code, on
GCC 13.3 and Clang 18.1:

- The exact DESIGN_NOTES item-3 repro (`ILP_RETURN(int)` in a `long`-returning
  function) — which that doc records as *"silent pun, wrong at -O0, accidentally
  right at -O2, not caught by UBSan"* — now aborts at `-O2` with a message naming
  both types.
- The nested-propagation hop pun (untyped inner `ILP_RETURN(int)` into an
  `ILP_FOR_T(long)` outer) is caught by the same mechanism with no extra code.
- The same-size pun (`int` stored, `float` recovered) is caught — this is why the
  check is type-identity-based, not size/alignment-based.
- Matched-type programs (including nested propagation) run silently and correctly;
  the full 352-assertion test suite passes with the check **active** (the default
  in this repo's test builds, which never define `NDEBUG`).
- Under `-DNDEBUG`, generated assembly is **byte-identical** to the pre-change
  headers on both compilers, and `sizeof(ilp::SmallStorage) == ilp::arch::sbo_size`
  (verified via static_assert) — the zero-cost claim is proven, not asserted.
- `ILP_MODE_SIMPLE` builds were unaffected at the time (they never touched
  `SmallStorage`, since `ILP_MODE_SIMPLE` lowered to a separate literal-`for`
  macro expansion). **Historical note:** this is no longer why SIMPLE mode is
  fine — [OPEN_ITEMS_PLAN.md](OPEN_ITEMS_PLAN.md) later deleted that separate
  expansion, so `ILP_MODE_SIMPLE` now goes through the exact same
  `SmallStorage`/macro layer as the default build, and this check is fully
  **active** there too (SIMPLE-mode test coverage went from 108 to 352
  assertions as a result — see DESIGN_NOTES.md item 1's resolution).
- `-DILP_DEBUG_TYPECHECK` force-enables the check even under `NDEBUG`.

**Correction to the "benchmarks build without NDEBUG" finding below:** verified
false on implementation. `benchmarks/CMakeLists.txt` doesn't override
`CMAKE_CXX_FLAGS_RELEASE`, and CMake's own default for that variable is
`-O3 -DNDEBUG` — confirmed by fetching Google Benchmark and inspecting the actual
`bench_reduce.cpp` compile command line, which does contain `-DNDEBUG`. The original
finding was based on a `grep` for the literal string in the CMakeLists.txt, which
missed CMake's implicit default. No change made to `benchmarks/CMakeLists.txt`.

**Implements:** DESIGN_NOTES item 3's proposed plan (debug-mode stored-vs-recovered
type check), extended per the branch review to cover `propagate_return`'s
cross-nesting-level extracts — which it does automatically, since every recovery
funnels through `SmallStorage::extract`.

---

## Design

### Gate

```cpp
// ctrl.hpp, right after the ILP_ALWAYS_INLINE block
#if !defined(ILP_NO_DEBUG_TYPECHECK) && (defined(ILP_DEBUG_TYPECHECK) || !defined(NDEBUG))
#define ILP_TYPECHECK_ENABLED 1
#include <cstdio>
#include <cstdlib>
#else
#define ILP_TYPECHECK_ENABLED 0
#endif
```

- **On by default in assert-style debug builds** (`NDEBUG` undefined) — which
  includes this repo's own test suite (tests override `CMAKE_CXX_FLAGS_RELEASE`
  without `-DNDEBUG`), so the entire existing suite exercises the checked path.
- `ILP_DEBUG_TYPECHECK` force-enables (e.g. for a checked release build);
  `ILP_NO_DEBUG_TYPECHECK` force-disables (for users who need debug-build layout
  identical to release — see the ODR caveat below).
- `<cstdio>`/`<cstdlib>` come back into `ctrl.hpp` **only under the gate** (they
  were deliberately removed when the runtime END-mismatch abort was deleted;
  release builds stay free of them).

### Type identity without RTTI

```cpp
// ilp::detail, next to always_false
#if ILP_TYPECHECK_ENABLED
template<typename T>
constexpr const char* type_name() {
#if defined(_MSC_VER) && !defined(__clang__)
    return __FUNCSIG__;
#else
    return __PRETTY_FUNCTION__;
#endif
}

struct TypeTag {
    const char* name;
};

template<typename T>
inline constexpr TypeTag type_tag_v{type_name<T>()};

[[noreturn]] inline void type_mismatch_abort(const char* stored, const char* recovered);
// fprintf both names + the fix ("match the types exactly, or use ILP_FOR_T /
// for_loop_typed"), reference docs/DESIGN_NOTES.md item 3, then std::abort().
#endif
```

The **address** of `type_tag_v<T>` is the identity — C++17 inline variables have
one address program-wide, so cross-TU comparison is exact, and string-literal
merging (not guaranteed) is irrelevant. The embedded compiler signature string
(`"... [with T = int]"`) names the type in the diagnostic without RTTI and without
constexpr string parsing; slightly verbose but unambiguous.

### SmallStorage changes (the only storage touched)

```cpp
struct SmallStorage {
    alignas(arch::sbo_size) char buffer[arch::sbo_size];
#if ILP_TYPECHECK_ENABLED
    const detail::TypeTag* ilp_debug_stored_tag = nullptr;
#endif

    // set(): after the placement-new, record  ilp_debug_stored_tag = &type_tag_v<U>;
    // extract<R>(): before the launder/read:
    //   using Rt = std::remove_cvref_t<R>;
    //   if (ilp_debug_stored_tag != nullptr && ilp_debug_stored_tag != &type_tag_v<Rt>)
    //       type_mismatch_abort(ilp_debug_stored_tag->name, type_tag_v<Rt>.name);
};
```

The `nullptr` guard is defensive (extract is only reachable when `has_return` is
true, i.e. after a `set`), and it also makes value-initialized storage
(`ForResult{false, {}}`) inert.

Why this one hook covers everything:

| Path | Coverage |
|---|---|
| Top-level `ILP_END_RETURN` recovery (Proxy → `extract<R>`, incl. the MSVC branch and the `std::optional<T>` conversion, which extracts `T`) | checked at extract |
| Nested untyped → typed hop (`propagate_return` calls `extract<R2>`) | checked at extract |
| Nested untyped → untyped hop (byte-copy `outer.storage = r.storage`) | the tag member rides along in the implicit copy; check happens at the final recovery — correct, since the value legitimately transits untouched |
| Nested typed → untyped hop (`outer.return_with(r.storage.extract())`) | `TypedStorage::extract` returns the true `R1`; `SmallStorage::set(R1)` records the tag fresh |
| Typed → typed (any hop) and `ILP_FOR_T` top-level | **no tag needed** — `TypedStorage<R>` is not type-erased; a wrong `ILP_RETURN` type there is a real conversion or a compile error already (item 3's original observation) |

`ForCtrl::return_with` funnels into `SmallStorage::set` — no change needed there
or anywhere else. `ctrl.hpp` is the only library file modified.

### Behavior on mismatch

Loud stderr message naming both types (compiler-signature strings) and the fix,
then `std::abort()` — assert semantics, debug builds only. The release-mode
contract is **unchanged**: types must match; mismatches remain the documented pun.
This check converts "silently wrong value in production" into "aborts in any
debug/test run that exercises the path".

---

## Caveats to document (README + header comment)

1. **Layout differs between checked and unchecked builds** (`SmallStorage` grows by
   one pointer). Mixing TUs compiled with and without `NDEBUG` that both use
   ilp_for inline functions is an ODR violation with real consequences here — the
   same class of issue as MSVC's `_ITERATOR_DEBUG_LEVEL`. One sentence in the
   README; `ILP_NO_DEBUG_TYPECHECK` is the escape hatch for projects that must mix.
2. **Debug-mode codegen changes** (observed: the asm probe actually got *smaller*,
   90→78 instructions, presumably from altered inlining) — expected and fine;
   the zero-cost guarantee applies to `NDEBUG` builds, where asm is byte-identical.

---

## Benchmarks and NDEBUG — checked during implementation, turned out fine

`benchmarks/CMakeLists.txt` doesn't explicitly pass `-DNDEBUG` anywhere in its own
text, which looked concerning on a `grep`. Verified empirically instead of assumed:
`benchmarks/CMakeLists.txt` never overrides `CMAKE_CXX_FLAGS_RELEASE`, and CMake's
own default for that variable is `-O3 -DNDEBUG` for GCC/Clang — confirmed by
fetching Google Benchmark and building the real target, then inspecting the actual
compile command line (`... -O3 -DNDEBUG -std=gnu++20 -O3 -march=native ...`). So
`NDEBUG` was already defined for benchmark builds before this change, and the type
check is already off there by default. No change made.

---

## Tests

1. **Positive:** the entire existing suite runs with the check enabled (no NDEBUG
   in test flags) — zero new tests needed for the happy path, though add one
   explicit test to `test_function_api.cpp` asserting a matched-type
   `for_loop`+`return_with`+extract round-trip still works (self-documenting).
2. **Negative — new `tests/runtime_fail/` harness**, mirroring `compile_fail/`:
   each `.cpp` declares its expectation on line 1, the driver compiles **and
   runs** it:
   - `// RUN_ABORT: <substring expected on stderr>` — must exit abnormally
     (SIGABRT) *and* stderr must contain the substring.
   - `// RUN_OK` — must compile, run, and exit 0 (harness control).
   - optional `// EXTRA_FLAGS: <flags>` line 2, appended to the compile command.
   Cases:
   - `pun_toplevel.cpp` — the item-3 repro verbatim (`ILP_RETURN(int)`, function
     returns `long`). Expect `"stored a value"` + abort.
   - `pun_nested_hop.cpp` — untyped inner `ILP_RETURN(int)` inside
     `ILP_FOR_T(long)` outer. Expect abort.
   - `pun_same_size.cpp` — `ILP_RETURN(int)` recovered as `float`. Expect abort
     (proves type-identity, not size, is checked).
   - `control_ok.cpp` — matched types, `RUN_OK`.
   - `release_layout.cpp` — `RUN_OK` with `EXTRA_FLAGS: -DNDEBUG`, containing
     `static_assert(sizeof(ilp::SmallStorage) == ilp::arch::sbo_size)` — the
     zero-overhead proof, permanently in CI.
   Wire as a CMake target (`runtime-fail-tests`) + a default-mode leg in
   `test_all_modes.sh`, forwarding `CXX` the same way `compile_fail` now does.
3. **ASM regression step (manual, note in commit):** regenerate the probe with
   `-O2 -march=x86-64-v3 -DNDEBUG` against pre/post headers on GCC and Clang and
   confirm byte-identical output (prototype already confirmed; re-verify on the
   final code).
4. Full matrix as usual: GCC+Clang, default+SIMPLE modes, compile-fail harness,
   ASan/UBSan subset.

---

## Documentation

- **DESIGN_NOTES item 3:** append `**Status: resolved (debug-mode check).**` —
  the release-mode contract is unchanged, but debug builds now abort with both
  type names; covers nested propagation hops per the cross-reference already in
  that item. Note the runtime_fail tests as the regression net.
- **README:**
  - "Large Return Types" section: one paragraph — debug builds verify the stored
    type matches the recovered type and abort with a message naming both;
    `ILP_DEBUG_TYPECHECK` / `ILP_NO_DEBUG_TYPECHECK` knobs; the mixed-NDEBUG ODR
    caveat.
  - "Nested Loops" mode-parity caveat: add one sentence — debug builds catch the
    cross-level type mismatch automatically.
- **This file:** flip Status to implemented at the end.

## Implementation order

1. `ctrl.hpp`: gate + tag machinery + `SmallStorage` changes (prototype-exact).
2. `benchmarks/CMakeLists.txt`: verify NDEBUG status — turned out to be a no-op,
   see above.
3. `tests/runtime_fail/` harness + cases + CMake/`test_all_modes.sh` wiring.
4. Verification: full matrix + NDEBUG asm identity check.
5. Docs last.
