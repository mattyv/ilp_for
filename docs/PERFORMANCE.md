# Performance Notes

The numbers, and the reasoning behind them: where the speedup comes from, and where you shouldn't expect one. The generated code matches what you'd get hand-writing the unrolled main-loop-plus-remainder pattern yourself.

## ILP_FOR Benchmarks

Apple M2, Clang 19, 10M elements, `-O3 -march=native`

| Loop Type | Simple | Pragma | ILP | Speedup |
|-----------|--------|--------|-----|---------|
| `ILP_FOR` with `ILP_BREAK` | 1.46ms | 1.46ms | 0.94ms | **1.56x** |
| `ILP_FOR` with `ILP_RETURN` | 1.68ms | 1.51ms | 0.94ms | **1.79x** |
| `ILP_FOR` with `ILP_CONTINUE` | 1.96ms | 1.72ms | 1.49ms | **1.31x** |
| `ILP_FOR_RANGE` with `ILP_BREAK` | 2.21ms | - | 0.94ms | **2.35x** |

Note: for early-exit patterns, `#pragma unroll` performs no better than the simple loop — the per-iteration bounds checks it inserts cancel the unroll. See [Why Not Pragma Unroll?](PRAGMA_UNROLL.md).

### x86: AMD Ryzen AI 9 HX PRO 370 (Zen 5), 10M elements, `-O3 -march=native`

Note this is mobile Zen 5 (Strix Point), which executes AVX-512 double-pumped on 256-bit
units ([TechPowerUp](https://www.techpowerup.com/324873/amd-strix-point-soc-zen-5-and-zen-5c-cpu-cores-have-256-bit-fpu-datapaths),
[Chips and Cheese](https://chipsandcheese.com/p/amds-strix-point-zen-5-hits-mobile)) —
desktop Zen 5's full 512-bit datapath may shift the SIMD-sensitive numbers below.

Clang 20:

| Loop Type | Simple | Pragma | ILP | Speedup |
|-----------|--------|--------|-----|---------|
| `ILP_FOR` with `ILP_BREAK` | 1.02ms | 0.73ms | 0.50ms | **2.04x** |
| `ILP_FOR` with `ILP_RETURN` | 1.02ms | 0.70ms | 0.49ms | **2.07x** |
| `ILP_FOR` with `ILP_CONTINUE` | 1.10ms | 1.03ms | 1.02ms | **1.08x** |
| `ILP_FOR_RANGE` with `ILP_BREAK` | 1.18ms | - | 0.49ms | **2.40x** |

GCC 15:

| Loop Type | Simple | Pragma | ILP | Speedup |
|-----------|--------|--------|-----|---------|
| `ILP_FOR` with `ILP_BREAK` | 0.30ms | 0.32ms | 0.50ms | 0.60x |
| `ILP_FOR` with `ILP_RETURN` | 1.04ms | 0.66ms | 0.53ms | **1.95x** |
| `ILP_FOR` with `ILP_CONTINUE` | 0.34ms | 0.33ms | 13.39ms | 0.03x |
| `ILP_FOR_RANGE` with `ILP_BREAK` | 0.32ms | - | 0.47ms | 0.68x |

Two honest caveats on GCC 15 / Zen 5:

- **GCC's early-break vectorizer wins the simple loops.** GCC 14+ auto-vectorizes the plain `break`/`continue`/range loops here (~0.3ms), and the ILP transformation blocks that vectorization. On GCC, measure before reaching for ILP on these patterns; `ILP_RETURN` still delivers ~2x.
- **The `ILP_CONTINUE` benchmark hits GCC's predicate-order branch-miss cliff** (13.39ms): an unpredictable parity check ahead of a rarely-true threshold check, through the macro expansion. Order the most-selective condition first, or mark the enclosing function `ILP_FLATTEN` — see the GCC predicate-order caveat in [PRAGMA_UNROLL.md](PRAGMA_UNROLL.md).

### Why ILP_RETURN is Faster

`ILP_RETURN` lets the compiler hoist the comparisons ahead of the conditional return logic, so they execute in parallel:

```cpp
// ILP pattern - comparisons run in parallel
ILP_FOR(auto i, 0, n, 4) {
    if (data[i] == target) ILP_RETURN(i);
} ILP_END_RETURN;

// Conceptually:
bool b0 = data[i+0] == target;  // parallel
bool b1 = data[i+1] == target;  // parallel
bool b2 = data[i+2] == target;  // parallel
bool b3 = data[i+3] == target;  // parallel
// then sequential check and return
```

## When ILP Helps

**Good candidates:**
- Loops with early exit (`ILP_BREAK`, `ILP_RETURN`)
- Bodies with loop-carried dependency chains (min, max, running products)
- Searches whose comparisons can be evaluated in parallel

**Skip ILP for:**
- Straight-line sums with no early exit — the auto-vectorizer already wins
- Anything the compiler demonstrably optimizes well on its own

```cpp
// Use ILP - early exit benefits from parallel evaluation
ILP_FOR(auto i, 0, n, 4) {
    if (expensive_check(data[i])) ILP_RETURN(result);
} ILP_END_RETURN;

// Skip ILP - compiler auto-vectorizes better
int sum = std::accumulate(data.begin(), data.end(), 0);
```

## `ilp::find_if` Benchmarks

`ilp::find_if` (see the README's [`ilp::find_if`](../README.md#ilpfind_if--vectorizable-first-match-search)
section) targets a different case from the rest of this library: a *trivially
vectorizable* first-match search. It resolves to one of two shapes at its N=0
default, chosen per compiler **and** ISA (`ilp::detail::find_if_resolved_block`,
`ilp_for/detail/find.hpp`) — not a single blockcheck default, because on GCC
15+ the plain scalar loop wins or near-ties once the hardware supports it. An
explicit `N` always forces the blockcheck shape regardless of compiler/ISA
(unchanged from the first cut of this feature) — the escape hatch for the
mid-tier-ISA cases noted below, where the plain loop doesn't always win.

**All numbers below are one canonical run** of
`benchmarks/find_block_sweep.cpp` (10M elements, break-at-midpoint,
best-of-25, this machine — AMD Zen 5 / Ryzen AI 9 HX PRO 370) — the same run
is cited in `ilp_for/detail/find.hpp`'s comments, so the two stay consistent.
Re-run that tool rather than spot-checking a single new measurement against
these numbers: run-to-run noise at the microsecond scale of the u8/u16 cells
is real (single-digit percent swings are normal), and some cells (noted
below) are inherently marginal/erratic rather than cleanly settled.

### Default-resolution strategy

| Compiler / ISA | Default | Why |
|---|---|---|
| Clang (any) | Blockcheck, `B = clamp(256 / sizeof(T), 32, 128)` | No measured cliff at any size; scaling B to the element's byte width keeps the win within 6% of the measured best from 1-byte to 8-byte elements. |
| GCC 15+ **and** SSE4.1+/AArch64 | Plain scalar loop (no block phase) | GCC's own early-break loop vectorizer wins or near-ties blockcheck on **native** (AVX-512) hardware. On mid-tier ISAs (SSE4.2/AVX2) it's a net win on balance, but blockcheck still wins outright at some element sizes there — see the v2/v3 breakdown below; an explicit `N` is the escape hatch. |
| GCC (any, without that ISA gate), MSVC, unknown | Blockcheck, `B = min(16, 64 / sizeof(T))` | Conservative SLP-safe sizing (see "Why two shapes" below); fixes a real bug in the first cut, where a flat `N=16` put 8-byte elements right at GCC's SLP cliff edge. |

The ISA gate exists because GCC's early-break vectorizer needs `ptest`-class
branch-on-vector-mask hardware — present from SSE4.1 on x86, assumed present
on AArch64 (the feature was developed by Arm) — and because it does **not**
fire below that: at `-march=x86-64` (SSE2-only) baseline, GCC's early-break
vectorizer never engages, so defaulting to the plain loop there would silently
regress to an unvectorized scalar scan. **32-bit ARM (`__ARM_NEON` without
`__aarch64__`) is deliberately excluded from the gate**: whether GCC 15's
early-break vectorizer fires on armv7/NEON is unverified on this machine, and
guessing "yes" is the riskier direction — it would silently cost armv7 users
the blockcheck fallback that's known to work, in exchange for an unconfirmed
win. `N > 16` still triggers GCC's deprecation warning when passed explicitly,
unchanged from before.

### Measured tables

**Clang 20**, `-march=native` (AVX-512) / `-march=x86-64` (SSE2) baseline — blockcheck always wins, never cliffs:

| Type | simple (native) | best N (native) | resolved default (native) | simple (baseline) | best N (baseline) |
|---|---|---|---|---|---|
| u8  | 0.974ms | N64 0.039ms | N128 0.040ms (+2.6%) | 0.974ms | N128 0.059ms |
| u16 | 0.979ms | N256 0.067ms | N128 0.068ms (+1.5%) | 0.977ms | N64 0.103ms |
| u32 | 0.984ms | N256 0.190ms | N64 0.201ms (+5.8%) | 0.995ms | N16 0.287ms |
| u64 | 1.005ms | N128 0.535ms | N32 0.552ms (+3.2%) | 1.017ms | N32 0.608ms (**N64 cliffs to 1.006ms**) |

**GCC 15**, `-march=native` (AVX-512) / `-march=x86-64` (SSE2) baseline — plain loop wins or near-ties at every size on native; blockcheck (N8) wins on baseline, where the vectorizer doesn't fire:

| Type | simple (native) | best blockcheck (native) | simple (baseline) | best blockcheck (baseline) |
|---|---|---|---|---|
| u8  | 0.038ms | N16 0.184ms | 0.973ms | N8 0.551ms |
| u16 | 0.091ms | N16 0.178ms | 0.979ms | N8 0.556ms |
| u32 | 0.261ms | N16 0.277ms (simple ~6% ahead) | 0.983ms | N8 0.564ms |
| u64 | 0.600ms | N8 0.595ms (near-tie either way) | 1.013ms | N8 0.686ms |

GCC's blockcheck N32+ cliffs hard at every element size on every ISA tier measured (1.1-2.1ms, worse than N16 by roughly 4-10x) — see "Why two shapes" below.

One line each, GCC 15, `-march=x86-64-v2` (SSE4.2) / `-march=x86-64-v3` (AVX2), and g++-13 native (AVX-512, but no early-break loop vectorizer regardless of ISA — GCC 15 is the minimum version this gate applies to):

- **v2 (SSE4.2), mixed — this is the case the plain-loop default trades away:** plain loop wins only u8 (0.122ms vs blockcheck-best 0.243ms, ~2x); blockcheck wins u16 (0.189ms vs simple 0.247ms, ~23% faster) and u64 (0.644ms vs simple 1.001ms — the vectorizer isn't meaningfully firing for u64 here, ~36% faster); u32 is a near-tie with simple marginally ahead (0.496ms vs blockcheck-best 0.504ms, ~1.6%). An explicit `N=16` recovers the u16/u64 wins for callers who've measured their own case.
- **v3 (AVX2):** plain loop wins u8 (0.061ms, ~4x) and u16 (0.127ms vs 0.183ms, ~30% faster); blockcheck is slightly ahead at u32 (0.284ms vs simple 0.293ms, ~3%) and u64 is a near-tie tilted to simple (0.590ms vs blockcheck-best 0.598ms).
- **g++-13 native:** always blockcheck (GCC<15 never gets the early-break loop vectorizer, on any ISA) — N16 wins u8/u16/u32 (0.183/0.174/0.271-0.278ms); u64's N16 is erratic across repeated runs (0.703-1.006ms against a ~1.0-1.02ms scalar loop — right at the SLP cliff edge, sometimes barely ahead of simple, sometimes essentially tied) — N8 is stable and reliably faster (0.568-0.627ms across the same runs). This instability is exactly why the SLP byte-cap (`gcc_slp_max_bytes = 64`) picks N8 over a flat N16 for 8-byte elements: 16 elements x 8 bytes = 128 bytes/block is past the one-native-vector-group SLP limit, landing right on the cliff's unstable edge rather than clearly on either side of it.

Reproduce any of the above (or re-tune for new hardware) with `benchmarks/find_block_sweep.cpp` / the `find_block_sweep` CMake target — a standalone, dependency-free sweep tool that prints exactly this table shape.

### Why two shapes

GCC only vectorizes the blockcheck shape via **SLP** (superword-level
parallelism) over the fully-unrolled inner block — at most one native vector
group. `-fopt-info-vec-all` on a 2-vector-group block (e.g. N=32 for a 4-byte
element on a 16-wide AVX-512 target) reports:

```
missed: may need non-SLP handling
...
not vectorized: unsupported outerloop form
```

i.e. the outer block loop itself is never vectorized or unrolled further —
once the inner "any match in this block?" scan needs more than one vector
register's worth of compares, GCC gives up on the whole block, and the
generated code degrades to something close to scalar per-block overhead
without the SIMD payoff. That's the N>16 (or N > 64/sizeof(T)) cliff.

GCC 15's **early-break loop vectorizer** (distinct from SLP; extended to
runtime trip counts by [commit r14-6822](https://gcc.gnu.org/pipermail/gcc-cvs/2023-December/395758.html))
instead recognizes the *plain* scalar early-exit loop shape directly and emits
a dedicated mask-and-test kernel — for a `uint8_t` compare, four instructions
process a full 64-byte vector:

```
vmovdqa64 (mem), %zmm
vpcmpub   $imm, %zmm, %zmm, %k
kortestq  %k, %k
je        <next-block>
```

Clang's loop vectorizer, by contrast, handles the blockcheck shape directly at
any block size — no SLP cap, hence no cliff, and no reason to prefer the plain
loop over it.

Defaults were measured for integral element types; a different element size,
alignment, or access pattern (e.g. non-contiguous ranges like `std::deque`,
which this function also supports correctly, just without vectorization) may
shift the optimal block size.

**Apple Silicon (Apple Clang + NEON) is unmeasured.** Expected reasonable
defaults: Clang has no measured cliff at any size, and the byte-scaled rule
keeps blocks at or below 16 NEON (128-bit) vectors even at the largest clamp
(128 elements of a 1-byte type = 16 vectors), which is inside the measured-safe
zone even at SSE2 width. Run the `find_block_sweep` target there before
re-tuning the constants in `ilp_for/detail/find.hpp`.

**Sources.** The capability these defaults tune is the compiler's early-break
vectorizer, not the CPU (contrast the scalar unroll factors in `cpu_profiles/`,
which cite [uops.info](https://uops.info) and
[Agner Fog's tables](https://www.agner.org/optimize/instruction_tables.pdf)):

- GCC 14 introduced vectorization of loops with early exits
  ([commit r14-6822](https://gcc.gnu.org/pipermail/gcc-cvs/2023-December/395758.html));
  GCC 15 extended it to unknown trip counts, e.g. `std::find`-shaped loops
  ([Arm: GCC 15 — Continuously Improving](https://developer.arm.com/community/arm-community-blogs/b/tools-software-ides-blog/posts/gcc-15-continuously-improving)).
- The GCC SLP cliff (blockcheck N past one native vector group) and the
  SSE4.1/AArch64 ISA gate for the early-break loop vectorizer are **not
  documented upstream** — both are empirical, derived from `-fopt-info-vec-all`
  and the measurements above; this file is the primary source. The gate
  deliberately excludes 32-bit ARM/NEON as unverified (see the strategy table
  above).
- Per-microarchitecture vector datapath widths (relevant if re-tuning N):
  [Agner Fog's microarchitecture guide](https://www.agner.org/optimize/) and the
  vendor optimization guides.

## Why Not `#pragma unroll`?

For loops with early exit, the compiler cannot determine the trip count (SCEV has no answer for a loop that might `break`), so `#pragma unroll` conservatively re-checks the bounds after every element — and those checks consume exactly the win the unroll was meant to deliver.

**Result:** for early-exit patterns, pragma unroll performs the same as a simple loop.

See [PRAGMA_UNROLL.md](PRAGMA_UNROLL.md) for assembly evidence and technical details.
