// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <utility>

#include "ctrl.hpp"
#include "mode.hpp"

namespace ilp {

    namespace detail {

        // --- find_if default block-size strategy --------------------------
        //
        // The right default depends on COMPILER *and* ISA, not just compiler:
        // GCC 15's early-break loop vectorizer (which beats the blockcheck
        // shape on native/high-end ISAs, once it's available) requires
        // ptest-class branch-on-vector-mask hardware (SSE4.1 on x86; assumed
        // available on AArch64, since the feature was developed by Arm - see
        // the gate below for why 32-bit ARM/NEON is deliberately excluded).
        // See docs/PERFORMANCE.md for the full measured tables and the
        // -fopt-info-vec-all mechanism (SLP-only blockcheck vectorization vs.
        // GCC's dedicated early-break loop vectorizer) behind this split.
        //
        // CANONICAL MEASUREMENT RUN cited throughout this file and
        // docs/PERFORMANCE.md: 10M elements, break-at-midpoint, best-of-25,
        // this machine (AMD Zen 5 / Ryzen AI 9 HX PRO 370),
        // `benchmarks/find_block_sweep.cpp`, -O3 -march=native unless noted.
        // Re-run that tool (rather than eyeballing new numbers against these
        // comments) before changing any constant below - run-to-run noise at
        // the microsecond scale of the u8/u16 cells is real (single-digit
        // percent), so a one-off measurement is not grounds for retuning.

        // Clang 20, all measured element sizes and both native (AVX-512) and
        // -march=x86-64 (SSE2) baseline: blockcheck always wins, never cliffs.
        // Best block size scales with element BYTE width, clamped to
        // [32, 128] elements - within 6% of the measured best at every cell
        // (worst case is u32 native: resolved N64 0.201ms vs best-measured
        // N256 0.190ms):
        //   u8:  N128 (0.040ms native / 0.059ms baseline) vs simple ~1.0ms
        //   u16: N128 (0.068ms native / 0.105ms baseline)
        //   u32: N64  (0.201ms native / 0.293ms baseline)
        //   u64: N32  (0.552ms native / 0.608ms baseline; N64 CLIFFS to 1.006ms baseline)
        inline constexpr std::size_t clang_block_bytes = 256; // target block width in bytes
        inline constexpr std::size_t clang_block_min_elems = 32;
        inline constexpr std::size_t clang_block_max_elems = 128;

        // GCC 15+ with a ptest-capable ISA: the early-break LOOP vectorizer
        // wins or near-ties blockcheck on NATIVE (AVX-512) hardware at every
        // measured element size:
        //   u8: simple 0.038 vs blockcheck-best 0.184; u16: 0.091 vs 0.178;
        //   u32: 0.261 vs 0.277 (simple ~6% ahead); u64: 0.600 vs 0.595
        //   (near-tie, simple still preferred for uniformity - no
        //   block-phase code to maintain/emit).
        // ACCEPTED TRADEOFF: on mid-tier ISAs (x86-64-v2/SSE4.2, x86-64-v3/
        // AVX2) the vectorizer fires but doesn't always win - blockcheck
        // still wins outright at some element sizes there (e.g. v2 u16: simple
        // 0.247 vs blockcheck-best 0.189, ~23% faster; v2 u64: simple 1.001
        // (vectorizer effectively not helping) vs blockcheck-best 0.644, ~36%
        // faster). The SSE4.1 gate is deliberately ALL-OR-NOTHING per ISA
        // tier rather than per-element-size: an explicit N (forcing
        // blockcheck) remains the escape hatch for callers on v2/v3 hardware
        // who've measured their own element size and want the blockcheck win.
        // See docs/PERFORMANCE.md for the full v2/v3 breakdown.
        // Gated on GCC>=15 (r14-6822 extended runtime-trip-count early-break
        // vectorization to GCC 15) AND an ISA that actually fires it: at
        // -march=x86-64 (SSE2) baseline the vectorizer does NOT fire (simple
        // stays scalar, ~1.0ms) - the SSE4.1 gate below keeps that baseline
        // case on the (still-correct, if unvectorized) blockcheck path
        // instead of silently falling back to a slow scalar loop. 32-bit ARM
        // (__ARM_NEON without __aarch64__) is deliberately NOT included:
        // whether GCC 15's early-break vectorizer fires there is unverified,
        // and guessing wrong in this direction is the worse failure mode -
        // armv7 users would silently lose the blockcheck fallback that's
        // known to work, for an unconfirmed win. AArch64 is included because
        // it always implies NEON, a much narrower and better-supported target
        // than the 32-bit ARM/NEON permutation space.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 15 && (defined(__SSE4_1__) || defined(__aarch64__))
        inline constexpr bool gcc_prefers_plain_loop = true;
#else
        inline constexpr bool gcc_prefers_plain_loop = false;
#endif

        // Fallback blockcheck sizing for: GCC without the early-break
        // vectorizer (any GCC on baseline x86-64/SSE2; GCC < 15 on any ISA),
        // MSVC, and unknown compilers. GCC only vectorizes the blockcheck
        // shape via SLP over the fully-unrolled inner block - one native
        // vector group max (2+ vectors: "missed: may need non-SLP handling",
        // outer loop "not vectorized: unsupported outerloop form" - see
        // docs/PERFORMANCE.md). That caps the safe block size at 16 elements
        // OR 64 bytes, whichever is smaller. g++-13 native measured N=16 for
        // u64 (128 bytes/block, past the 64-byte SLP cap) as erratic and
        // sometimes worse than the plain scalar loop across repeated runs
        // (0.703-1.006ms against a ~1.0ms scalar loop - right at the cliff
        // edge); N=8 (64B/8, within the cap) is stable and reliably faster
        // (0.568-0.627ms across the same runs). g++-13/g++-15-baseline
        // best-of-N was N=8-16 uniformly across u8/u16/u32, so the elems cap
        // alone covers those; only u64 needs the byte cap to kick in.
        inline constexpr std::size_t gcc_slp_max_elems = 16;
        inline constexpr std::size_t gcc_slp_max_bytes = 64;

        // Resolves the N=0 (default) sentinel to a strategy for element type
        // T: a nonzero return is a blockcheck size; 0 means "use the plain
        // scalar loop, no block phase" (the GCC-15-plus-vectorizable-ISA
        // case above). Templated on T (not called with a runtime size)
        // because the strategy is a compile-time, per-instantiation decision
        // - it must be usable in an `if constexpr` guarding the block phase.
        template<typename T>
        constexpr std::size_t find_if_resolved_block() {
#if defined(__clang__) // must precede __GNUC__: clang defines both
            std::size_t b = clang_block_bytes / sizeof(T);
            if (b < clang_block_min_elems)
                b = clang_block_min_elems;
            if (b > clang_block_max_elems)
                b = clang_block_max_elems;
            return b;
#elif defined(__GNUC__)
            if constexpr (gcc_prefers_plain_loop) {
                return 0;
            } else {
                std::size_t b = gcc_slp_max_bytes / sizeof(T);
                if (b < 1)
                    b = 1;
                return b < gcc_slp_max_elems ? b : gcc_slp_max_elems;
            }
#else // MSVC & others: unmeasured -> the same conservative SLP-style sizing
            std::size_t b = gcc_slp_max_bytes / sizeof(T);
            if (b < 1)
                b = 1;
            return b < gcc_slp_max_elems ? b : gcc_slp_max_elems;
#endif
        }

        // Threshold above which an EXPLICIT block size N is flagged on GCC as
        // likely to fall off the measured SLP vectorization cliff (see
        // gcc_slp_max_elems above). Clang has no such cliff in the measured
        // range, so this warning is GCC-only. Unaffected by the strategy
        // resolution above: an EXPLICIT N always forces the blockcheck shape
        // (see find_if_impl), so this warning's applicability doesn't change
        // with GCC version or ISA.
        inline constexpr std::size_t find_block_gcc_cliff_threshold = 16;

#if defined(__GNUC__) && !defined(__clang__)
        template<std::size_t N>
        [[deprecated("Explicit find_if block size N > 16 has been measured to fall "
                     "off GCC's SLP auto-vectorization cliff. Prefer the default "
                     "block size, or N<=16.")]]
        constexpr void warn_gcc_find_block_cliff() {}
#endif

        // Deliberately NOT validate_unroll_factor<N>() (loops_common.hpp): that
        // helper's N>16 warning would fire on Clang's own default block sizes
        // (up to 128), which is not a Clang cliff. This validator only warns
        // for an EXPLICIT N > 16 on GCC, where the cliff was actually measured.
        template<std::size_t N, bool Explicit>
        constexpr void validate_find_block() {
            static_assert(N >= 1, "find_if block size N must be at least 1");
#if defined(__GNUC__) && !defined(__clang__)
            if constexpr (Explicit && N > find_block_gcc_cliff_threshold) {
                warn_gcc_find_block_cliff<N>();
            }
#endif
        }

        // Two-phase blockcheck scan: a branch-free "does this block contain a
        // match?" pass (vectorizes on GCC 15 / Clang 20), followed by a scalar
        // re-scan of the hit block (or the remainder) to locate the exact first
        // match. See docs/PERFORMANCE.md for the measured shape this mirrors.
        //
        // When the resolved strategy is "plain loop" (GCC 15+ on a
        // ptest-capable ISA, at the N=0 default only), the block phase is
        // compiled out entirely via `use_blockcheck` below, and this function
        // reduces to exactly the scalar finish loop - the same code path
        // Mode::Simple takes.
        //
        // Not constexpr - nothing else in this library is.
        //
        // ILP_ALWAYS_INLINE: the probe that validated this shape used an inline
        // comparison, not a lambda; force-inlining the predicate call site keeps
        // the block loop in the shape the vectorizer was measured to accept.
        template<std::size_t N, Mode M, std::ranges::random_access_range Range, typename Pred>
        ILP_ALWAYS_INLINE std::ranges::borrowed_iterator_t<Range> find_if_impl(Range&& range, Pred pred) {
            using T = std::ranges::range_value_t<Range>;
            constexpr std::size_t resolved = find_if_resolved_block<T>();
            // An explicit N always forces the blockcheck shape (unchanged
            // behavior); N=0 defers to the resolved per-compiler/per-ISA
            // strategy, which may itself choose the plain loop (resolved==0).
            constexpr bool use_blockcheck = (N != 0) || (resolved != 0);
            constexpr std::size_t B = (N != 0) ? N : (resolved != 0 ? resolved : std::size_t{1});

            if constexpr (N != 0) {
                validate_find_block<B, true>();
            } else if constexpr (resolved != 0) {
                validate_find_block<B, false>();
            }

            auto it = std::ranges::begin(range);
            const std::size_t size = std::ranges::size(range);
            std::size_t i = 0;

            if constexpr (M == Mode::Unrolled && use_blockcheck) {
                const std::size_t block_end = size - size % B;
                for (; i != block_end; i += B) {
                    bool any = false;
                    for (std::size_t j = 0; j < B; ++j) // branch-free: vectorizes
                        any |= static_cast<bool>(pred(it[i + j]));
                    if (any)
                        break; // hit block: scalar re-scan below finds the exact match
                }
            }

            for (; i < size; ++i)
                if (pred(it[i]))
                    break;

            // The search itself always runs against the real range (still alive
            // for the duration of this call, even if it's a temporary about to be
            // destroyed at the end of the caller's full-expression). Only the
            // RETURNED handle is downgraded to std::ranges::dangling for a
            // non-borrowed rvalue range, matching std::ranges::find_if's own
            // contract - the caller cannot safely dereference an iterator into a
            // range it doesn't own past this call.
            if constexpr (std::ranges::borrowed_range<Range>) {
                return it + static_cast<std::ranges::range_difference_t<Range>>(i);
            } else {
                return std::ranges::dangling{};
            }
        }

    } // namespace detail

    // Vectorizable first-match search: scans `range` for the first element
    // satisfying `pred`. At the N=0 (default) block size, the strategy is
    // resolved per-compiler and per-ISA (see detail::find_if_resolved_block
    // and docs/PERFORMANCE.md for the measured tables):
    //   - Clang: a two-phase blockcheck shape (branch-free block-level match
    //     test, then a scalar re-scan of the hit block), block size scaled to
    //     the element's byte width.
    //   - GCC 15+ on an ISA with ptest-class branch-on-vector-mask hardware
    //     (SSE4.1+ on x86; AArch64 on Arm - 32-bit ARM/NEON deliberately
    //     excluded, unverified): the PLAIN scalar loop - GCC's own
    //     early-break loop vectorizer wins or near-ties blockcheck on native
    //     (AVX-512) hardware; on mid-tier ISAs (SSE4.2/AVX2) it's a net win
    //     on balance but blockcheck still wins outright at some element
    //     sizes there (see docs/PERFORMANCE.md) - an explicit N remains the
    //     escape hatch for callers who've measured their own case.
    //   - Everything else (older GCC, GCC on a narrower ISA, MSVC, unknown):
    //     the blockcheck shape at a conservative SLP-safe block size.
    // An EXPLICIT N always forces the blockcheck shape, unaffected by the
    // strategy above - there is no cpu::Profile knob for N=0, because the
    // constraint being tuned is the auto-vectorizer/compiler, not the CPU
    // microarchitecture.
    //
    // Contract: `pred` must be pure. It may be invoked on elements past the
    // first match, and more than once per element - callers must not rely on
    // side effects or on a specific invocation count. An exception thrown from
    // `pred` propagates normally, but may occur after later elements were
    // already tested.
    //
    // Index-predicate searches (rather than element-predicate) can use
    // std::views::iota(0, n) as the range - see the README for the idiom.
    //
    // Returns std::ranges::borrowed_iterator_t<Range>: passing an rvalue
    // non-borrowed range (e.g. a temporary std::vector) yields
    // std::ranges::dangling, matching std::ranges::find_if's own contract.
    template<std::size_t N = 0, Mode M = default_mode, std::ranges::random_access_range Range, typename Pred>
        requires std::ranges::sized_range<Range> && std::indirect_unary_predicate<Pred, std::ranges::iterator_t<Range>>
    std::ranges::borrowed_iterator_t<Range> find_if(Range&& range, Pred pred) {
        return detail::find_if_impl<N, M>(std::forward<Range>(range), std::move(pred));
    }

} // namespace ilp
