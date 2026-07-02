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

namespace ilp {
    namespace detail {

        template<typename F, typename T>
        concept ForEachBody = std::invocable<F, T, EachCtrl&>;

        template<typename F, typename Ref>
        concept ForEachRangeBody = std::invocable<F, Ref, EachCtrl&>;

        template<std::size_t N, Mode M, std::integral T, typename F>
            requires ForEachBody<F, T>
        void for_each_impl(T start, T end, F&& body) {
            validate_unroll_factor<N>();
            EachCtrl ctrl;
            T i = start;

            if constexpr (M == Mode::Unrolled) {
                for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
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

        template<std::size_t N, Mode M, std::ranges::random_access_range Range, typename F>
            requires ForEachRangeBody<F, std::ranges::range_reference_t<Range>>
        void for_each_range_impl(Range&& range, F&& body) {
            validate_unroll_factor<N>();
            EachCtrl ctrl;
            auto it = std::ranges::begin(range);
            auto size = std::ranges::size(range);
            std::size_t i = 0;

            if constexpr (M == Mode::Unrolled) {
                for (; i + N <= size; i += N) {
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
            requires ForUntypedCtrlBody<F, T>
        ForResult for_loop_untyped_impl(T start, T end, F&& body) {
            validate_unroll_factor<N>();
            ForCtrl ctrl;
            T i = start;

            if constexpr (M == Mode::Unrolled) {
                for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
                    for (std::size_t j = 0; j < N; ++j) {
                        body(i + static_cast<T>(j), ctrl);
                        if (!ctrl.ok) [[unlikely]]
                            return ForResult{ctrl.return_set, std::move(ctrl.storage)};
                    }
                }
            }

            for (; i < end; ++i) {
                body(i, ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return ForResult{ctrl.return_set, std::move(ctrl.storage)};
            }

            return ForResult{false, {}};
        }

        template<typename R, std::size_t N, Mode M, std::integral T, typename F>
            requires ForTypedCtrlBody<F, T, R>
        ForResultTyped<R> for_loop_typed_impl(T start, T end, F&& body) {
            validate_unroll_factor<N>();
            ForCtrlTyped<R> ctrl;
            T i = start;

            if constexpr (M == Mode::Unrolled) {
                for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
                    for (std::size_t j = 0; j < N; ++j) {
                        body(i + static_cast<T>(j), ctrl);
                        if (!ctrl.ok) [[unlikely]]
                            return ForResultTyped<R>{ctrl.return_set, std::move(ctrl.storage)};
                    }
                }
            }

            for (; i < end; ++i) {
                body(i, ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return ForResultTyped<R>{ctrl.return_set, std::move(ctrl.storage)};
            }

            return ForResultTyped<R>{false, {}};
        }

        template<std::size_t N, Mode M, std::ranges::random_access_range Range, typename F>
        void for_loop_range_impl(Range&& range, F&& body) {
            validate_unroll_factor<N>();
            using Ref = std::ranges::range_reference_t<Range>;
            constexpr bool has_ctrl = ForRangeCtrlBody<F, Ref>;

            auto it = std::ranges::begin(range);
            auto size = std::ranges::size(range);
            std::size_t i = 0;

            if constexpr (has_ctrl) {
                LoopCtrl<void> ctrl;

                if constexpr (M == Mode::Unrolled) {
                    for (; i + N <= size && ctrl.ok; i += N) {
                        for (std::size_t j = 0; j < N && ctrl.ok; ++j) {
                            body(it[i + j], ctrl);
                        }
                    }
                }

                for (; i < size && ctrl.ok; ++i) {
                    body(it[i], ctrl);
                }
            } else {
                static_assert(ForRangeBody<F, Ref>, "Lambda must be invocable with (Ref) or (Ref, LoopCtrl<void>&)");

                if constexpr (M == Mode::Unrolled) {
                    for (; i + N <= size; i += N) {
                        for (std::size_t j = 0; j < N; ++j) {
                            body(it[i + j]);
                        }
                    }
                }

                for (; i < size; ++i) {
                    body(it[i]);
                }
            }
        }

        template<std::size_t N, Mode M, std::ranges::random_access_range Range, typename F>
            requires ForRangeUntypedCtrlBody<F, std::ranges::range_reference_t<Range>>
        ForResult for_loop_range_untyped_impl(Range&& range, F&& body) {
            validate_unroll_factor<N>();
            ForCtrl ctrl;
            auto it = std::ranges::begin(range);
            auto size = std::ranges::size(range);
            std::size_t i = 0;

            if constexpr (M == Mode::Unrolled) {
                for (; i + N <= size; i += N) {
                    for (std::size_t j = 0; j < N; ++j) {
                        body(it[i + j], ctrl);
                        if (!ctrl.ok) [[unlikely]]
                            return ForResult{ctrl.return_set, std::move(ctrl.storage)};
                    }
                }
            }

            for (; i < size; ++i) {
                body(it[i], ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return ForResult{ctrl.return_set, std::move(ctrl.storage)};
            }

            return ForResult{false, {}};
        }

        template<typename R, std::size_t N, Mode M, std::ranges::random_access_range Range, typename F>
            requires ForRangeTypedCtrlBody<F, std::ranges::range_reference_t<Range>, R>
        ForResultTyped<R> for_loop_range_typed_impl(Range&& range, F&& body) {
            validate_unroll_factor<N>();
            ForCtrlTyped<R> ctrl;
            auto it = std::ranges::begin(range);
            auto size = std::ranges::size(range);
            std::size_t i = 0;

            if constexpr (M == Mode::Unrolled) {
                for (; i + N <= size; i += N) {
                    for (std::size_t j = 0; j < N; ++j) {
                        body(it[i + j], ctrl);
                        if (!ctrl.ok) [[unlikely]]
                            return ForResultTyped<R>{ctrl.return_set, std::move(ctrl.storage)};
                    }
                }
            }

            for (; i < size; ++i) {
                body(it[i], ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return ForResultTyped<R>{ctrl.return_set, std::move(ctrl.storage)};
            }

            return ForResultTyped<R>{false, {}};
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
        // Macro entries always use default_mode: when ILP_MODE_SIMPLE is defined
        // these macros aren't compiled at all (macros_simple.hpp takes over), so
        // no per-call Mode plumbing is needed here.

        template<std::size_t N, std::integral T, typename F>
        NoResult macro_for(T start, T end, F&& body, end_tag_t) {
            for_each_impl<N, default_mode>(start, end, std::forward<F>(body));
            return {};
        }

        template<std::size_t N, std::integral T, typename F>
        ForResult macro_for(T start, T end, F&& body, end_return_tag_t) {
            return for_loop_untyped_impl<N, default_mode>(start, end, std::forward<F>(body));
        }

        template<std::size_t N, std::ranges::random_access_range Range, typename F>
        NoResult macro_for_range(Range&& range, F&& body, end_tag_t) {
            for_each_range_impl<N, default_mode>(std::forward<Range>(range), std::forward<F>(body));
            return {};
        }

        template<std::size_t N, std::ranges::random_access_range Range, typename F>
        ForResult macro_for_range(Range&& range, F&& body, end_return_tag_t) {
            return for_loop_range_untyped_impl<N, default_mode>(std::forward<Range>(range), std::forward<F>(body));
        }

        template<typename ElementT, LoopType LT, std::integral T, typename F>
        NoResult macro_for_auto(T start, T end, F&& body, end_tag_t) {
            for_each_impl<optimal_N<LT, ElementT>, default_mode>(start, end, std::forward<F>(body));
            return {};
        }

        template<typename ElementT, LoopType LT, std::integral T, typename F>
        ForResult macro_for_auto(T start, T end, F&& body, end_return_tag_t) {
            return for_loop_untyped_impl<optimal_N<LT, ElementT>, default_mode>(start, end, std::forward<F>(body));
        }

        template<typename ElementT, LoopType LT, std::ranges::random_access_range Range, typename F>
        NoResult macro_for_range_auto(Range&& range, F&& body, end_tag_t) {
            for_each_range_impl<optimal_N<LT, ElementT>, default_mode>(std::forward<Range>(range), std::forward<F>(body));
            return {};
        }

        template<typename ElementT, LoopType LT, std::ranges::random_access_range Range, typename F>
        ForResult macro_for_range_auto(Range&& range, F&& body, end_return_tag_t) {
            return for_loop_range_untyped_impl<optimal_N<LT, ElementT>, default_mode>(std::forward<Range>(range),
                                                                                        std::forward<F>(body));
        }

        // ILP_FOR_T closed with plain ILP_END (no ILP_RETURN in the body) is
        // pointless but legal today; the end_tag_t overload keeps it legal by
        // delegating to the untyped for_each_impl (R is unused in that path).
        template<typename R, std::size_t N, std::integral T, typename F>
        NoResult macro_for_typed(T start, T end, F&& body, end_tag_t) {
            for_each_impl<N, default_mode>(start, end, std::forward<F>(body));
            return {};
        }

        template<typename R, std::size_t N, std::integral T, typename F>
        ForResultTyped<R> macro_for_typed(T start, T end, F&& body, end_return_tag_t) {
            return for_loop_typed_impl<R, N, default_mode>(start, end, std::forward<F>(body));
        }

        template<typename R, std::size_t N, std::ranges::random_access_range Range, typename F>
        NoResult macro_for_range_typed(Range&& range, F&& body, end_tag_t) {
            for_each_range_impl<N, default_mode>(std::forward<Range>(range), std::forward<F>(body));
            return {};
        }

        template<typename R, std::size_t N, std::ranges::random_access_range Range, typename F>
        ForResultTyped<R> macro_for_range_typed(Range&& range, F&& body, end_return_tag_t) {
            return for_loop_range_typed_impl<R, N, default_mode>(std::forward<Range>(range), std::forward<F>(body));
        }

        template<typename ElementT, typename R, LoopType LT, std::integral T, typename F>
        NoResult macro_for_typed_auto(T start, T end, F&& body, end_tag_t) {
            for_each_impl<optimal_N<LT, ElementT>, default_mode>(start, end, std::forward<F>(body));
            return {};
        }

        template<typename ElementT, typename R, LoopType LT, std::integral T, typename F>
        ForResultTyped<R> macro_for_typed_auto(T start, T end, F&& body, end_return_tag_t) {
            return for_loop_typed_impl<R, optimal_N<LT, ElementT>, default_mode>(start, end, std::forward<F>(body));
        }

        template<typename ElementT, typename R, LoopType LT, std::ranges::random_access_range Range, typename F>
        NoResult macro_for_range_typed_auto(Range&& range, F&& body, end_tag_t) {
            for_each_range_impl<optimal_N<LT, ElementT>, default_mode>(std::forward<Range>(range),
                                                                        std::forward<F>(body));
            return {};
        }

        template<typename ElementT, typename R, LoopType LT, std::ranges::random_access_range Range, typename F>
        ForResultTyped<R> macro_for_range_typed_auto(Range&& range, F&& body, end_return_tag_t) {
            return for_loop_range_typed_impl<R, optimal_N<LT, ElementT>, default_mode>(std::forward<Range>(range),
                                                                                         std::forward<F>(body));
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
