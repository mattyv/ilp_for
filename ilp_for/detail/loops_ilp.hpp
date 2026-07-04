// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <utility>

#include "ctrl.hpp"
#include "loops_common.hpp"
#include "mode.hpp"

// unrolled_block_end (below) computes `end - start` by casting both signed operands
// to their unsigned counterpart and subtracting - the two's-complement bit pattern
// makes this unsigned, wrapping subtraction equal the true mathematical difference
// even when start is negative (e.g. start=-5, end=5: 5u - 4294967291u wraps to the
// correct 10, since -5 % 2^32 == 4294967291). This is well-defined per the C++
// standard (unsigned arithmetic wraps, it is not UB) and is the standard technique
// for computing a signed difference without risking signed-overflow UB - but
// Clang's opt-in `-fsanitize=unsigned-integer-overflow` flags any unsigned wrap as
// suspicious regardless of intent, so it needs an explicit, scoped opt-out. GCC has
// no equivalent check and warns on the unrecognized attribute under -Wall -Wextra,
// so this is Clang-only.
#if defined(__clang__)
#define ILP_NO_SANITIZE_UNSIGNED_OVERFLOW __attribute__((no_sanitize("unsigned-integer-overflow")))
#else
#define ILP_NO_SANITIZE_UNSIGNED_OVERFLOW
#endif

namespace ilp {
    namespace detail {

        // Largest B with start <= B <= end and (B - start) % N == 0, computed in
        // unsigned arithmetic. The naive block-loop bound `i + N <= end` overflows
        // (UB) for signed T when end is within N of its maximum; precomputing the
        // block end and iterating `i != block_end` avoids that while keeping a
        // single comparison in the hot loop.
        template<std::size_t N, std::integral T>
        ILP_ALWAYS_INLINE constexpr T ILP_NO_SANITIZE_UNSIGNED_OVERFLOW unrolled_block_end(T start, T end) {
            using U = std::make_unsigned_t<T>;
            if (start >= end)
                return start; // empty range: block loop must not run
            const U total = static_cast<U>(end) - static_cast<U>(start);
            return static_cast<T>(static_cast<U>(start) + (total - total % static_cast<U>(N)));
        }

        // Shared Mode-aware main+remainder skeleton over an integral index range.
        // Every index-loop impl below is a thin wrapper choosing a ctrl type and
        // packaging the result; the unroll/remainder/early-exit shape lives only here.
        template<std::size_t N, Mode M, std::integral T, typename Ctrl, typename F>
        ILP_ALWAYS_INLINE void index_loop_core(T start, T end, Ctrl& ctrl, F&& body) {
            validate_unroll_factor<N>();
            T i = start;

            if constexpr (M == Mode::Unrolled) {
                const T block_end = unrolled_block_end<N>(start, end);
                for (; i != block_end; i += static_cast<T>(N)) {
                    for (std::size_t j = 0; j < N; ++j) {
                        body(i + static_cast<T>(j), ctrl);
                        if (!ctrl.ok) [[unlikely]]
                            return;
                    }
                }
            }

            for (; i < end; ++i) {
                body(i, ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return;
            }
        }

        // Same skeleton over a random-access range (size_t indices).
        template<std::size_t N, Mode M, typename Ctrl, std::ranges::random_access_range Range, typename F>
        ILP_ALWAYS_INLINE void range_loop_core(Range&& range, Ctrl& ctrl, F&& body) {
            validate_unroll_factor<N>();
            auto it = std::ranges::begin(range);
            const std::size_t size = std::ranges::size(range);
            std::size_t i = 0;

            if constexpr (M == Mode::Unrolled) {
                const std::size_t block_end = size - size % N;
                for (; i != block_end; i += N) {
                    for (std::size_t j = 0; j < N; ++j) {
                        body(it[i + j], ctrl);
                        if (!ctrl.ok) [[unlikely]]
                            return;
                    }
                }
            }

            for (; i < size; ++i) {
                body(it[i], ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return;
            }
        }

        template<std::size_t N, Mode M, std::integral T, typename F>
            requires ForEachBody<F, T>
        void for_each_impl(T start, T end, F&& body) {
            EachCtrl ctrl;
            index_loop_core<N, M>(start, end, ctrl, std::forward<F>(body));
        }

        template<std::size_t N, Mode M, std::ranges::random_access_range Range, typename F>
            requires ForEachRangeBody<F, std::ranges::range_reference_t<Range>>
        void for_each_range_impl(Range&& range, F&& body) {
            EachCtrl ctrl;
            range_loop_core<N, M>(std::forward<Range>(range), ctrl, std::forward<F>(body));
        }

        template<std::size_t N, Mode M, std::integral T, typename F>
            requires ForUntypedCtrlBody<F, T>
        ForResult for_loop_untyped_impl(T start, T end, F&& body) {
            ForCtrl ctrl;
            index_loop_core<N, M>(start, end, ctrl, std::forward<F>(body));
            // Only move the storage when a value was actually stored; otherwise the
            // buffer holds indeterminate bytes that must not be copied.
            return ctrl.return_set ? ForResult{true, std::move(ctrl.storage)} : ForResult{false, {}};
        }

        template<typename R, std::size_t N, Mode M, std::integral T, typename F>
            requires ForTypedCtrlBody<F, T, R>
        ForResultTyped<R> for_loop_typed_impl(T start, T end, F&& body) {
            ForCtrlTyped<R> ctrl;
            index_loop_core<N, M>(start, end, ctrl, std::forward<F>(body));
            return ctrl.return_set ? ForResultTyped<R>{true, std::move(ctrl.storage)}
                                   : ForResultTyped<R>{false, {}};
        }

        template<std::size_t N, Mode M, std::ranges::random_access_range Range, typename F>
            requires ForRangeUntypedCtrlBody<F, std::ranges::range_reference_t<Range>>
        ForResult for_loop_range_untyped_impl(Range&& range, F&& body) {
            ForCtrl ctrl;
            range_loop_core<N, M>(std::forward<Range>(range), ctrl, std::forward<F>(body));
            return ctrl.return_set ? ForResult{true, std::move(ctrl.storage)} : ForResult{false, {}};
        }

        template<typename R, std::size_t N, Mode M, std::ranges::random_access_range Range, typename F>
            requires ForRangeTypedCtrlBody<F, std::ranges::range_reference_t<Range>, R>
        ForResultTyped<R> for_loop_range_typed_impl(Range&& range, F&& body) {
            ForCtrlTyped<R> ctrl;
            range_loop_core<N, M>(std::forward<Range>(range), ctrl, std::forward<F>(body));
            return ctrl.return_set ? ForResultTyped<R>{true, std::move(ctrl.storage)}
                                   : ForResultTyped<R>{false, {}};
        }

        template<std::size_t N, Mode M, std::ranges::random_access_range Range, typename F>
        auto for_loop_range_ret_simple_impl(Range&& range, F&& body) {
            validate_unroll_factor<N>();

            auto it = std::ranges::begin(range);
            auto end_it = std::ranges::end(range);
            auto size = std::ranges::size(range);
            using Sentinel = decltype(end_it);

            using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>, Sentinel>;

            if constexpr (std::is_same_v<R, bool>) {
                std::size_t i = 0;
                if constexpr (M == Mode::Unrolled) {
                    for (; i + N <= size; i += N) {
                        std::array<bool, N> matches;
                        for (std::size_t j = 0; j < N; ++j) {
                            matches[j] = body(it[i + j], end_it);
                        }

                        for (std::size_t j = 0; j < N; ++j) {
                            if (matches[j])
                                return it + (i + j);
                        }
                    }
                }
                for (; i < size; ++i) {
                    if (body(it[i], end_it))
                        return it + i;
                }
                return end_it;
            } else if constexpr (is_optional_v<R>) {
                std::size_t i = 0;
                if constexpr (M == Mode::Unrolled) {
                    for (; i + N <= size; i += N) {
                        std::array<R, N> results;
                        for (std::size_t j = 0; j < N; ++j) {
                            results[j] = body(it[i + j], end_it);
                        }

                        for (std::size_t j = 0; j < N; ++j) {
                            if (results[j].has_value())
                                return std::move(results[j]);
                        }
                    }
                }
                for (; i < size; ++i) {
                    R result = body(it[i], end_it);
                    if (result.has_value())
                        return result;
                }
                return R{};
            } else {
                std::size_t i = 0;
                if constexpr (M == Mode::Unrolled) {
                    for (; i + N <= size; i += N) {
                        std::array<R, N> results;
                        for (std::size_t j = 0; j < N; ++j) {
                            results[j] = body(it[i + j], end_it);
                        }

                        for (std::size_t j = 0; j < N; ++j) {
                            if (results[j] != end_it)
                                return std::move(results[j]);
                        }
                    }
                }
                for (; i < size; ++i) {
                    R result = body(it[i], end_it);
                    if (result != end_it)
                        return result;
                }
                return static_cast<R>(end_it);
            }
        }

        // --- Macro-layer entry points -------------------------------------
        //
        // ILP_FOR(...) { body } ILP_END; / ILP_END_RETURN; jointly form one call
        // to the matching macro_for* below, with the closing macro appending the
        // tag argument (end_tag_t or end_return_tag_t). Overload resolution on
        // that tag selects which ctrl type (EachCtrl vs ForCtrl/ForCtrlTyped) the
        // body lambda is instantiated against - which is what turns ILP_RETURN
        // used under ILP_END into a compile error (EachCtrl::return_with is
        // poisoned) instead of the historical runtime abort. See
        // docs/END_ENFORCEMENT_PLAN.md for the full design rationale.
        //
        // R = void selects the untyped (SBO) return path; a non-void R selects the
        // typed path (ILP_FOR_T family). A typed loop closed with plain ILP_END
        // (no ILP_RETURN in the body) is pointless but legal; the end_tag_t
        // overloads run the untyped break-only path and ignore R.
        //
        // The *_auto entries exist as distinct names (rather than the macros
        // passing optimal_N directly) so tooling can tell ILP_FOR_AUTO expansions
        // apart from ILP_FOR by callee name - the ilp-loop-analysis clang-tidy
        // check relies on this to skip loops that already use auto N selection.
        //
        // Macro entries always use default_mode: when ILP_MODE_SIMPLE is defined
        // these macros aren't compiled at all (macros_simple.hpp takes over), so
        // no per-call Mode plumbing is needed here.

        template<std::size_t N, typename R = void, std::integral T, typename F>
        NoResult macro_for(T start, T end, F&& body, end_tag_t) {
            for_each_impl<N, default_mode>(start, end, std::forward<F>(body));
            return {};
        }

        template<std::size_t N, typename R = void, std::integral T, typename F>
        auto macro_for(T start, T end, F&& body, end_return_tag_t) {
            if constexpr (std::is_void_v<R>)
                return for_loop_untyped_impl<N, default_mode>(start, end, std::forward<F>(body));
            else
                return for_loop_typed_impl<R, N, default_mode>(start, end, std::forward<F>(body));
        }

        template<std::size_t N, typename R = void, std::ranges::random_access_range Range, typename F>
        NoResult macro_for_range(Range&& range, F&& body, end_tag_t) {
            for_each_range_impl<N, default_mode>(std::forward<Range>(range), std::forward<F>(body));
            return {};
        }

        template<std::size_t N, typename R = void, std::ranges::random_access_range Range, typename F>
        auto macro_for_range(Range&& range, F&& body, end_return_tag_t) {
            if constexpr (std::is_void_v<R>)
                return for_loop_range_untyped_impl<N, default_mode>(std::forward<Range>(range),
                                                                    std::forward<F>(body));
            else
                return for_loop_range_typed_impl<R, N, default_mode>(std::forward<Range>(range),
                                                                     std::forward<F>(body));
        }

        template<typename ElementT, LoopType LT, typename R = void, std::integral T, typename F, typename Tag>
        auto macro_for_auto(T start, T end, F&& body, Tag tag) {
            return macro_for<optimal_N<LT, ElementT>, R>(start, end, std::forward<F>(body), tag);
        }

        template<typename ElementT, LoopType LT, typename R = void, std::ranges::random_access_range Range,
                  typename F, typename Tag>
        auto macro_for_range_auto(Range&& range, F&& body, Tag tag) {
            return macro_for_range<optimal_N<LT, ElementT>, R>(std::forward<Range>(range), std::forward<F>(body),
                                                               tag);
        }

    } // namespace detail

    // Break/continue-only loop: no ILP_RETURN-equivalent capability, hence no
    // return value to discard. Prefer this over for_loop when the body never
    // needs to return a value out of the enclosing function - it avoids the
    // `[[maybe_unused]] auto r = ...` boilerplate for_loop otherwise requires.
    template<std::size_t N = 4, Mode M = default_mode, std::integral T, typename F>
        requires detail::ForEachBody<F, T>
    void for_each(T start, T end, F&& body) {
        detail::for_each_impl<N, M>(start, end, std::forward<F>(body));
    }

    template<std::size_t N = 4, Mode M = default_mode, std::ranges::random_access_range Range, typename F>
        requires detail::ForEachRangeBody<F, std::ranges::range_reference_t<Range>>
    void for_each_range(Range&& range, F&& body) {
        detail::for_each_range_impl<N, M>(std::forward<Range>(range), std::forward<F>(body));
    }

    template<std::size_t N = 4, Mode M = default_mode, std::integral T, typename F>
        requires detail::ForUntypedCtrlBody<F, T>
    ForResult for_loop(T start, T end, F&& body) {
        return detail::for_loop_untyped_impl<N, M>(start, end, std::forward<F>(body));
    }

    template<typename R, std::size_t N = 4, Mode M = default_mode, std::integral T, typename F>
        requires detail::ForTypedCtrlBody<F, T, R>
    ForResultTyped<R> for_loop_typed(T start, T end, F&& body) {
        return detail::for_loop_typed_impl<R, N, M>(start, end, std::forward<F>(body));
    }

    template<std::size_t N = 4, Mode M = default_mode, std::ranges::random_access_range Range, typename F>
        requires detail::ForRangeUntypedCtrlBody<F, std::ranges::range_reference_t<Range>>
    ForResult for_loop_range(Range&& range, F&& body) {
        return detail::for_loop_range_untyped_impl<N, M>(std::forward<Range>(range), std::forward<F>(body));
    }

    template<typename R, std::size_t N = 4, Mode M = default_mode, std::ranges::random_access_range Range,
              typename F>
        requires detail::ForRangeTypedCtrlBody<F, std::ranges::range_reference_t<Range>, R>
    ForResultTyped<R> for_loop_range_typed(Range&& range, F&& body) {
        return detail::for_loop_range_typed_impl<R, N, M>(std::forward<Range>(range), std::forward<F>(body));
    }

    template<std::size_t N = 4, Mode M = default_mode, std::ranges::random_access_range Range, typename F>
    auto for_loop_range_ret_simple(Range&& range, F&& body) {
        return detail::for_loop_range_ret_simple_impl<N, M>(std::forward<Range>(range), std::forward<F>(body));
    }

    template<typename ElementT, LoopType LT, Mode M = default_mode, std::integral T, typename F>
        requires detail::ForUntypedCtrlBody<F, T>
    ForResult for_loop_auto(T start, T end, F&& body) {
        return for_loop<optimal_N<LT, ElementT>, M>(start, end, std::forward<F>(body));
    }

    template<typename ElementT, typename R, LoopType LT, Mode M = default_mode, std::integral T, typename F>
        requires detail::ForTypedCtrlBody<F, T, R>
    ForResultTyped<R> for_loop_typed_auto(T start, T end, F&& body) {
        return for_loop_typed<R, optimal_N<LT, ElementT>, M>(start, end, std::forward<F>(body));
    }

    template<typename ElementT, LoopType LT, Mode M = default_mode, std::ranges::random_access_range Range,
              typename F>
        requires detail::ForRangeUntypedCtrlBody<F, std::ranges::range_reference_t<Range>>
    ForResult for_loop_range_auto(Range&& range, F&& body) {
        return for_loop_range<optimal_N<LT, ElementT>, M>(std::forward<Range>(range), std::forward<F>(body));
    }

    template<typename ElementT, typename R, LoopType LT, Mode M = default_mode,
              std::ranges::random_access_range Range, typename F>
        requires detail::ForRangeTypedCtrlBody<F, std::ranges::range_reference_t<Range>, R>
    ForResultTyped<R> for_loop_range_typed_auto(Range&& range, F&& body) {
        return for_loop_range_typed<R, optimal_N<LT, ElementT>, M>(std::forward<Range>(range), std::forward<F>(body));
    }

} // namespace ilp
