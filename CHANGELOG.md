# Changelog

Notable, user-visible changes to `ilp_for`. Format loosely follows
[Keep a Changelog](https://keepachangelog.com/); versions are git tags.

## [Unreleased]

### Added

- `ilp::find_if(range, pred)`: a dedicated vectorizable first-match search
  primitive, separate from `ILP_FOR`/`ilp::for_loop`, unlike the per-lane
  exit-state tracking `ILP_FOR`/`ILP_BREAK` lower to (which cannot
  auto-vectorize). Its `N=0` default resolves to one of two shapes, chosen per
  **compiler and ISA** (element-size x ISA sweeps across GCC 13/15 and Clang
  20 at multiple `-march` levels falsified an earlier flat-N-per-compiler
  default): a two-phase "blockcheck" shape (branch-free block-level match
  test, then a scalar re-scan of the hit block), block size scaled to the
  element's byte width, on Clang; the *plain* scalar loop on GCC 15+ with a
  ptest-capable ISA (SSE4.1+/AArch64 - 32-bit ARM/NEON deliberately excluded
  as unverified), deferring to GCC's own early-break loop vectorizer, which
  wins or near-ties blockcheck on native (AVX-512) hardware but not
  universally on mid-tier ISAs (SSE4.2/AVX2), where an explicit `N` remains
  the escape hatch; a conservative SLP-safe blockcheck size elsewhere (older
  GCC, narrower ISAs, MSVC, unknown). An explicit `N` still always forces the
  blockcheck shape, unaffected by this strategy. See the README's
  [`ilp::find_if`](README.md#ilpfind_if--vectorizable-first-match-search)
  section and [docs/PERFORMANCE.md](docs/PERFORMANCE.md#ilpfind_if-benchmarks)
  for the full measured tables and the SLP-vs-loop-vectorizer mechanism.
- `benchmarks/find_block_sweep.cpp` (CMake target `find_block_sweep`): a
  standalone, dependency-free element-size x block-size sweep tool for
  reproducing (or re-tuning) `ilp::find_if`'s default block sizes on new
  hardware.

### Breaking changes (all `ILP_MODE_SIMPLE`-only; the default build is unchanged)

The two build modes were unified: `ILP_MODE_SIMPLE` no longer selects a
separate macro implementation that lowered loops to literal `for` statements —
it now only flips `ilp::default_mode` to `Mode::Simple` (remainder-only, one
bounds check per iteration), and the macro layer itself is identical in both
modes. Consequences for code that only ever compiled under `ILP_MODE_SIMPLE`:

- **Bare `return;` inside a loop body now means *continue*** (returns from the
  body lambda), matching the default build — previously it returned from the
  enclosing function. Use `ILP_RETURN(x)` / `ILP_CONTINUE`, which mean the same
  thing in both modes.
- **`ILP_FOR_RANGE*` now requires a sized random-access range** in SIMPLE builds too
  (the old literal range-for accepted any range). Iterating a `std::list`,
  `std::set`, or non-random-access view no longer compiles; those containers
  get no ILP benefit anyway — use an ordinary range-for.
- **Raw `break;` / `continue;` / `goto` out of a loop body no longer compile**
  in SIMPLE builds (they never compiled in the default build).
- **Single-stepping goes through one lambda frame** rather than a literal
  `for` loop — same caveat the function API's `Mode::Simple` always had.

In exchange, every compile-time and debug-mode guarantee below now applies
identically under `ILP_MODE_SIMPLE` (previously most were default-build-only).

### Breaking changes (return-storage hardening)

- The untyped `ILP_FOR` / `for_loop` return path now accepts only small,
  trivially-copyable values. Non-trivially-copyable values must use `ILP_FOR_T`
  or `for_loop_typed`; the previous raw relocation of such objects had undefined
  behavior even when they fit the inline buffer. Untyped result wrappers are now
  move-only so their inline payload cannot be relocated by implicit wrapper copies.
- Function-API loop callbacks must return exactly `void`. This rejects macro loops
  nested inside a function-API callback, whose expansion previously produced a
  non-`void` callback that could fall off its end.

### Changed

- Typed return storage now uses `std::optional<R>` to own object lifetime. Untyped
  storage is move-only, transports trivially-copyable bytes explicitly, and recovers
  values with `std::bit_cast`; oversized, reference, and non-trivially-copyable
  recovery types are rejected at compile time.
- Unroll factors that cannot be represented by the integral index type are rejected
  at compile time, and block bounds avoid overflowing `i + N` arithmetic.
- **`ILP_END` vs `ILP_END_RETURN` mismatch is now a compile-time error** with a
  message naming the fix. Previously a body that called `ILP_RETURN` but was
  closed with plain `ILP_END` compiled and aborted at runtime — and only on
  inputs that actually took the return path.
- **Nested `ILP_RETURN` now returns from the enclosing function at any
  nesting depth**, with each enclosing loop closed by `ILP_END_RETURN` carrying
  the value one level outward (an enclosing `ILP_END` on the path is a compile
  error). Previously the value silently escaped only as far as the enclosing
  loop's body lambda.
- Macro-internal identifiers renamed from the reserved `_ilp_*` pattern to
  `ilp_detail_*` (only observable if you referenced expansion internals).

### Added

- **Function API parity**: `ilp::for_each` / `ilp::for_each_range`
  (break/continue-only, `void`-returning), `ctrl.break_loop()` /
  `ctrl.return_with(x)`, and a per-call `ilp::Mode` template argument for
  de-ILPing a single loop without the global define.
- **Debug-mode type check** on the type-erased SBO return path: storing one
  type and recovering another (a silent byte reinterpretation in release)
  aborts with a message naming both types in any build without `NDEBUG`.
  Knobs: `ILP_DEBUG_TYPECHECK` forces it on, `ILP_NO_DEBUG_TYPECHECK` off.
- **Debug-mode swallowed-result detection**: a loop result holding a value
  that is destroyed without ever being converted (the signature of a macro
  loop incorrectly nested inside a function-API lambda) aborts with an
  actionable message instead of silently discarding the value.
- **`-Wshadow` silence on nested loops**: the macro-definition region of
  `ilp_for.hpp` is marked `#pragma GCC system_header`, so nested `ILP_FOR`
  no longer trips `-Wshadow`/`-Wshadow-all` while real shadowing in user code
  still warns. Opt out with `ILP_NO_SYSTEM_HEADER`; note the PCH caveat in the
  header. clang-tidy users need `HeaderFilterRegex: '.*'` / `SystemHeaders:
  true` (this repo ships a root `.clang-tidy` with both).
- **`ILP_FLATTEN`**: opt-in `[[gnu::flatten]]` function annotation working
  around a GCC missed optimization where independent body predicates fail to
  fuse through the macro expansion (see the GCC predicate-order caveat in
  `docs/PRAGMA_UNROLL.md`).
- CI: compile-fail/runtime-fail regression harnesses now run in CI on GCC and
  Clang in both build modes; the `ilp-loop-analysis` clang-tidy check was
  repaired (its matcher predated the macro-entry-point rename) and its job
  hardened.

### Removed

- Unused internal remnants: `LoopCtrl`, `no_return_t`, `for_result_t`, the signed/
  unsigned concept aliases, and the uncalled accumulator-width warning helpers.
- The undocumented `for_loop_range_ret_simple` prototype and implementation; the
  supported search API is `ilp::find_if`.
- The unused `ilp::iota` wrapper and its wrapper-only tests; use the C++20
  `std::views::iota` directly.
- `ilp_for/detail/macros_simple.hpp` (the separate `ILP_MODE_SIMPLE`
  implementation — see Breaking changes).
- The Axiom formal-specification experiment (`knowledge/`, `external/axiom`
  submodule, extraction config, and README section).

### Docs

- README and satellite docs (`ILP.md`, `EXAMPLES.md`, `PERFORMANCE.md`,
  `PRAGMA_UNROLL.md`) reworked; `DESIGN_NOTES.md` items 1–5 all resolved or
  mitigated with their resolutions recorded.
- Godbolt examples regenerated to match the current library line-for-line,
  with fresh share links and a generator script
  (`godbolt_examples/make_godbolt_links.py`).
- Compiler state-of-play (LLVM multi-exit unrolling, Apple Silicon early-continue
  unrolling, the SCEV limitation) documented with dated citations.

## [v0.7] and earlier

Pre-changelog. See the git tag history.
