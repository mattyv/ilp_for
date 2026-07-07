# Why Not Just `#pragma unroll`?

Compilers *will* unroll a loop with `break`/`continue`/`return` if you ask with `#pragma unroll`. The question is not whether it unrolls — it's **how many instructions each element costs once it has**.

---

## The Real Issue: Per-Iteration Bounds Checks

Adding `#pragma unroll` to an early-exit loop obliges the compiler to preserve exact break semantics. It cannot determine the trip count (SCEV has no answer for a loop that might `break`), so it protects itself with a bounds check after **every unrolled element** — the unrolled body is bigger, but each element still pays the full loop overhead.

### Assembly Evidence (x86-64 Clang)

**Pragma Unroll:**
```asm
.LBB0_2:                                ; main loop
    cmp  dword ptr [rdi + 4*rax], edx   ; load & compare element 0
    ja   .LBB0_18                       ; exit if > threshold
    cmp  r9, rax                        ; <-- bounds check (i < n-1?)
    je   .LBB0_12                       ; <-- exit if at end
    cmp  dword ptr [rdi + 4*rax + 4], edx ; element 1
    ja   .LBB0_13
    cmp  r8, rax                        ; <-- bounds check (again!)
    je   .LBB0_12
    cmp  dword ptr [rdi + 4*rax + 8], edx ; element 2
    ja   .LBB0_15
    cmp  rcx, rax                       ; <-- bounds check (again!)
    je   .LBB0_12
    cmp  dword ptr [rdi + 4*rax + 12], edx ; element 3
    ja   .LBB0_17
    add  rax, 4
    cmp  rsi, rax
    jne  .LBB0_2
```

**ILP_FOR:**
```asm
.LBB1_9:                                ; main loop
    cmp  dword ptr [rdi + 4*rax - 8], edx  ; element 0
    ja   .LBB1_17
    cmp  dword ptr [rdi + 4*rax - 4], edx  ; element 1
    ja   .LBB1_15
    cmp  dword ptr [rdi + 4*rax], edx      ; element 2
    ja   .LBB1_18
    cmp  dword ptr [rdi + 4*rax + 4], edx  ; element 3
    ja   .LBB1_16
    lea  rcx, [rax + 4]
    add  rax, 6
    cmp  rax, rsi                       ; <-- bounds check only HERE
    mov  rax, rcx
    jbe  .LBB1_9
```

### Assembly Evidence (ARM64 Clang)

**Pragma Unroll:**
```asm
LBB2_2:                                 ; main loop
    ldur w12, [x8, #-8]    ; load 0
    cmp  w12, w2
    b.hi LBB2_12           ; exit if > threshold
    cmp  x11, x0           ; <-- bounds check (i < n?)
    b.eq LBB2_10           ; <-- exit if at end
    ldur w12, [x8, #-4]    ; load 1
    cmp  w12, w2
    b.hi LBB2_13
    cmp  x10, x0           ; <-- another bounds check
    b.eq LBB2_10
    ... (repeated for each element)
```

**ILP_FOR:**
```asm
LBB0_8:                                 ; main loop
    ldur w9, [x10, #-8]    ; load 0
    cmp  w9, w2
    b.hi LBB0_6            ; exit if > threshold
    ldur w9, [x10, #-4]    ; load 1
    cmp  w9, w2
    b.hi LBB0_14
    ldr  w9, [x10]         ; load 2
    cmp  w9, w2
    b.hi LBB0_15
    ldr  w9, [x10, #4]     ; load 3
    cmp  w9, w2
    b.hi LBB0_16
    ; bounds check only HERE, after all 4 elements
    cmp  x11, x1
    b.ls LBB0_8
```

**Result:** Pragma has ~6 instructions per element, ILP_FOR has ~4. This accounts for the ~1.5x speedup.

[View on Godbolt](https://godbolt.org/z/Mh4aTP5j7)

---

## Why Can't the Compiler Optimize This?

The compiler uses **Scalar Evolution (SCEV)** to analyze loop trip counts. For a loop like:

```cpp
for (size_t i = 0; i < n; ++i) {
    if (data[i] > threshold) break;  // Early exit
    ++count;
}
```

SCEV cannot say how many iterations will run — the answer lives in the data. The compiler must assume the loop could exit at any element and plan accordingly.

**Without break:** SCEV knows exactly `n` iterations will run. The compiler can:
- Unroll cleanly into a main loop + tail loop
- Vectorize with SIMD (both Clang and GCC do this)
- Bounds check only at the end of each unrolled block

**With break:** The trip count is unknown. The compiler must:
- Check bounds after each element to preserve exact semantics
- Cannot speculatively execute past a potential exit point
- Falls back to "interleaved" unrolling instead of "strip-mining"

---

## Could This Be Fixed Upstream?

Probably, in principle. Nothing about "unknown trip count" *requires* a bounds
check per element — it only requires a bounds check per unrolled *block*, which
is exactly the code `ILP_FOR` hand-generates. An unroller that speculatively
executes a full block of N iterations and only re-checks the exit condition
after the block (rolling back or masking off the speculatively-executed tail
elements past the true exit point, since the loop body here has no
externally-visible side effects to undo before the check — `ILP_FOR`'s own
correctness argument for exactly this pattern) is a known, implementable shape;
it's a missed optimization in current SCEV-driven unrolling, not a fundamental
impossibility. This is a phase-ordering / cost-model gap, similar in spirit to
other unroll-heuristic gaps compilers have closed over time.

If LLVM or GCC ever taught their unrollers to do this, the gap `ILP_FOR`
exists to fill would shrink or close outright — and that would be a genuinely
good outcome: this library's job is to make a hand-rolled workaround for a
compiler limitation unnecessary to write by hand, not to be a permanent
alternative to the compiler doing its job. Worst case if that ever happens,
`ILP_FOR` degrades to emitting the same code the compiler would produce on its
own (see [Where ilp_for loses](../README.md#where-ilp_for-loses) for the class
of loop where this is already largely true today).

### State of play (as of late 2025)

The gap is still open on general targets, though there's movement:

- **The infrastructure exists but is switched off.** LLVM has carried
  runtime-unrolling support for multiple-exit loops (`-unroll-runtime-multi-exit`,
  with prologue/epilogue remainder handling) for years, but it's a hidden option
  defaulting to *off* — the cost model was never changed to enable it in normal
  compilation ([D107381](https://reviews.llvm.org/D107381)). So on a stock
  x86-64 or generic AArch64 target, an early-exit loop still gets the
  per-element bounds checks shown above.
- **Vendor-specific unrolling has started to appear.** In Dec 2024, LLVM gained
  runtime unrolling of loops with early-*continues* on Apple Silicon (A14–A16,
  M4) — [llvm/llvm-project#118499](https://github.com/llvm/llvm-project/pull/118499)
  — plus a companion pass for small load/store loops
  ([#118317](https://github.com/llvm/llvm-project/pull/118317)). Note the
  distinction: an early-`continue` doesn't break the trip count (the loop still
  runs to `n`), so this targets branch-prediction and memory-level parallelism,
  *not* the per-block-vs-per-element bounds-check problem that a `break`/`return`
  creates. It's a sign the door is open, not a fix for this specific case — and
  it's gated to one vendor's out-of-order cores.
- **The specific limitation is still reported as unresolved.** As of Oct 2025,
  LLVM still declines to runtime-unroll loops whose exit trip count SCEV can't
  prove non-wrapping
  ([llvm-project#165354](http://www.mail-archive.com/llvm-bugs@lists.llvm.org/msg93415.html)) —
  the direct descendant of the SCEV limitation described above.

None of this changes the recommendation today; it's here so the "missed
optimization, not a fundamental limit" claim above stays honest and checkable.

**GCC predicate-order caveat (as of Jul 2026, confirmed on 14.3.0 and 15.2.0):**
When a loop body has two or more independent predicates, GCC can fail to fuse
them through `ILP_FOR`'s macro expansion the way it does for a hand-written
loop. The macro's nested lambda layers *are* inlined before GCC's `ifcombine`
pass (the one that fuses independent conditions into a single branch) — but
not by the *early* inliner, so the body reaches ifcombine in a shape its
pattern-match rejects, and the predicates are never fused. (The exact IL
property that blocks the match isn't pinned down; what is confirmed is that
forcing *early* inlining fixes it, while merely marking the wrapper functions
`always_inline` — which also inlines them before ifcombine — does not.) If the
loop body's *first* condition is poorly predictable (e.g. a 50/50 parity check)
and a later, almost-always-false condition (e.g. a rarely-true threshold check)
is checked second, the unfused coin-flip check is left as the per-element
branch. Once the data exceeds cache, this shows up as a ~10-15x throughput
cliff from branch misprediction alone (measured: ~0.3 G/s vs. ~4.8 G/s on one
such loop, ~26% vs. ~0% branch-misses). Clang is unaffected — it emits the
fused, branchless form for this shape through the macro expansion (verified
Clang 20).

Two independent fixes, either is sufficient:
- **Reorder the body:** put the most-predictable or most-selective condition
  first, so the coin-flip check is no longer the one left exposed.
- **Force early inlining:** mark the enclosing function `ILP_FLATTEN` (see
  `ilp_for.hpp`). This expands to `[[gnu::flatten]]` on GCC/Clang, which
  force-inlines the whole `ILP_FOR` call tree into the annotated function
  early, letting ifcombine fuse the predicates (and, under `-march=native`,
  auto-vectorize the recovered loop — so the measured speedup above includes
  vectorization unlocked by the fusion, not fusion alone). No-op on other
  compilers.

  Two caveats on `ILP_FLATTEN`: (1) it only takes effect when `NDEBUG` is
  defined (or `-DILP_NO_DEBUG_TYPECHECK` is set) — otherwise the debug
  typecheck layer keeps the predicates unfusable even with flatten, and the
  annotation silently does nothing. Release builds define `NDEBUG`, so they
  are fine; plain `-O3` debug-ish builds are not. (2) `[[gnu::flatten]]`
  force-inlines *every* call the annotated function makes, so apply it to a
  small function containing the hot loop — on a large function, or one with
  heavy unrelated calls, it costs code size and compile time.

We believe this is a GCC missed-optimization, not a fundamental limit or a
correctness bug: the generated code is correct, just slow, and Clang fuses the
same predicates through the identical code with no hint. Forcing early inlining
recovers it on GCC too. We have *not* pinned the exact IL property that blocks
the fuse, and we have not reported it upstream — so treat `ILP_FLATTEN` and
predicate reordering as workarounds for current GCC, not a permanent tax.

---

## Verification: Loops Without Break

For loops **without** early exit, all approaches produce identical SIMD code:

```cpp
// All three compile to the same SIMD (ld4.4s, cmhs.4s, etc.)
size_t count_simple(const uint32_t* data, size_t n, uint32_t threshold);
size_t count_pragma(const uint32_t* data, size_t n, uint32_t threshold);
size_t count_ilp(const uint32_t* data, size_t n, uint32_t threshold);
```

**Conclusion:** `ILP_FOR` only earns its keep on loops with early exit. Without `break`/`return`, write the plain loop — the compiler already produces optimal code.

---

## Portability

Pragma syntax varies by compiler:

| Compiler | Syntax | Notes |
|----------|--------|-------|
| Clang | `#pragma clang loop unroll_count(N)` | Reliable |
| GCC | `#pragma GCC unroll N` | Reliable |
| MSVC | None | No equivalent pragma |
| Intel | `#pragma unroll(N)` | ICC/ICX only |

For portable code:
```cpp
#if defined(__clang__)
    #pragma clang loop unroll_count(4)
#elif defined(__GNUC__)
    #pragma GCC unroll 4
#endif
for (size_t i = 0; i < n; ++i) { ... }
```

Or just use `ILP_FOR` which works everywhere.

---

## When Does It Matter?

| Pattern | `#pragma unroll` | ILP_FOR | Winner |
|---------|------------------|---------|--------|
| Simple sum (no break) | SIMD | SIMD | Either |
| Early exit (`break`/`return`) | Bounds check per element | Bounds check per block | **ILP_FOR (~1.5x)** |
| Loops without early exit | SIMD | SIMD | Either |

---

## Summary

| Issue | Pragma Unroll | ILP_FOR |
|-------|---------------|---------|
| Bounds checks | Per element | Per block |
| Trip count needed? | No (but costly) | No |
| Portability | Compiler-specific | Universal |
| SIMD for simple loops | Yes | Yes |

**Use ILP_FOR for:**
- Loops with `break`, `continue`, or `return`

**Skip ILP_FOR for:**
- Simple loops without early exit (compilers handle these well)

---

## References

- [GCC Loop-Specific Pragmas](https://gcc.gnu.org/onlinedocs/gcc/Loop-Specific-Pragmas.html)
- [LLVM Loop Metadata](https://llvm.org/docs/TransformMetadata.html)
- [Godbolt Example: Pragma vs ILP](https://godbolt.org/z/Mh4aTP5j7)
