# Changelog

Notable, user-visible changes to `ilp_for`. Format loosely follows
[Keep a Changelog](https://keepachangelog.com/); versions are git tags.

## [Unreleased]

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
- **`ILP_FOR_RANGE*` now requires a random-access range** in SIMPLE builds too
  (the old literal range-for accepted any range). Iterating a `std::list`,
  `std::set`, or non-random-access view no longer compiles; those containers
  get no ILP benefit anyway — use an ordinary range-for.
- **Raw `break;` / `continue;` / `goto` out of a loop body no longer compile**
  in SIMPLE builds (they never compiled in the default build).
- **Single-stepping goes through one lambda frame** rather than a literal
  `for` loop — same caveat the function API's `Mode::Simple` always had.

In exchange, every compile-time and debug-mode guarantee below now applies
identically under `ILP_MODE_SIMPLE` (previously most were default-build-only).

### Changed

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
