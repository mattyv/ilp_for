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

namespace ilp {
    namespace detail {

        template<std::size_t N, std::integral T, typename F>
            requires ForUntypedCtrlBody<F, T>
        ForResult for_loop_untyped_impl(T start, T end, F&& body) {
            validate_unroll_factor<N>();
            ForCtrl ctrl;
            T i = start;

            for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
                for (std::size_t j = 0; j < N; ++j) {
                    body(i + static_cast<T>(j), ctrl);
                    if (!ctrl.ok) [[unlikely]]
                        return ForResult{ctrl.return_set, std::move(ctrl.storage)};
                }
            }

            for (; i < end; ++i) {
                body(i, ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return ForResult{ctrl.return_set, std::move(ctrl.storage)};
            }

            return ForResult{false, {}};
        }

        template<typename R, std::size_t N, std::integral T, typename F>
            requires ForTypedCtrlBody<F, T, R>
        ForResultTyped<R> for_loop_typed_impl(T start, T end, F&& body) {
            validate_unroll_factor<N>();
            ForCtrlTyped<R> ctrl;
            T i = start;

            for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
                for (std::size_t j = 0; j < N; ++j) {
                    body(i + static_cast<T>(j), ctrl);
                    if (!ctrl.ok) [[unlikely]]
                        return ForResultTyped<R>{ctrl.return_set, std::move(ctrl.storage)};
                }
            }

            for (; i < end; ++i) {
                body(i, ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return ForResultTyped<R>{ctrl.return_set, std::move(ctrl.storage)};
            }

            return ForResultTyped<R>{false, {}};
        }

        template<std::size_t N, std::ranges::random_access_range Range, typename F>
        void for_loop_range_impl(Range&& range, F&& body) {
            validate_unroll_factor<N>();
            using Ref = std::ranges::range_reference_t<Range>;
            constexpr bool has_ctrl = ForRangeCtrlBody<F, Ref>;

            auto it = std::ranges::begin(range);
            auto size = std::ranges::size(range);
            std::size_t i = 0;

            if constexpr (has_ctrl) {
                LoopCtrl<void> ctrl;

                for (; i + N <= size && ctrl.ok; i += N) {
                    for (std::size_t j = 0; j < N && ctrl.ok; ++j) {
                        body(it[i + j], ctrl);
                    }
                }

                for (; i < size && ctrl.ok; ++i) {
                    body(it[i], ctrl);
                }
            } else {
                static_assert(ForRangeBody<F, Ref>, "Lambda must be invocable with (Ref) or (Ref, LoopCtrl<void>&)");

                for (; i + N <= size; i += N) {
                    for (std::size_t j = 0; j < N; ++j) {
                        body(it[i + j]);
                    }
                }

                for (; i < size; ++i) {
                    body(it[i]);
                }
            }
        }

        template<std::size_t N, std::ranges::random_access_range Range, typename F>
            requires ForRangeUntypedCtrlBody<F, std::ranges::range_reference_t<Range>>
        ForResult for_loop_range_untyped_impl(Range&& range, F&& body) {
            validate_unroll_factor<N>();
            ForCtrl ctrl;
            auto it = std::ranges::begin(range);
            auto size = std::ranges::size(range);
            std::size_t i = 0;

            for (; i + N <= size; i += N) {
                for (std::size_t j = 0; j < N; ++j) {
                    body(it[i + j], ctrl);
                    if (!ctrl.ok) [[unlikely]]
                        return ForResult{ctrl.return_set, std::move(ctrl.storage)};
                }
            }

            for (; i < size; ++i) {
                body(it[i], ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return ForResult{ctrl.return_set, std::move(ctrl.storage)};
            }

            return ForResult{false, {}};
        }

        template<typename R, std::size_t N, std::ranges::random_access_range Range, typename F>
            requires ForRangeTypedCtrlBody<F, std::ranges::range_reference_t<Range>, R>
        ForResultTyped<R> for_loop_range_typed_impl(Range&& range, F&& body) {
            validate_unroll_factor<N>();
            ForCtrlTyped<R> ctrl;
            auto it = std::ranges::begin(range);
            auto size = std::ranges::size(range);
            std::size_t i = 0;

            for (; i + N <= size; i += N) {
                for (std::size_t j = 0; j < N; ++j) {
                    body(it[i + j], ctrl);
                    if (!ctrl.ok) [[unlikely]]
                        return ForResultTyped<R>{ctrl.return_set, std::move(ctrl.storage)};
                }
            }

            for (; i < size; ++i) {
                body(it[i], ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return ForResultTyped<R>{ctrl.return_set, std::move(ctrl.storage)};
            }

            return ForResultTyped<R>{false, {}};
        }

        template<std::size_t N, std::ranges::random_access_range Range, typename F>
        auto for_loop_range_ret_simple_impl(Range&& range, F&& body) {
            validate_unroll_factor<N>();

            auto it = std::ranges::begin(range);
            auto end_it = std::ranges::end(range);
            auto size = std::ranges::size(range);
            using Sentinel = decltype(end_it);

            using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>, Sentinel>;

            if constexpr (std::is_same_v<R, bool>) {
                std::size_t i = 0;
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
                for (; i < size; ++i) {
                    if (body(it[i], end_it))
                        return it + i;
                }
                return end_it;
            } else if constexpr (is_optional_v<R>) {
                std::size_t i = 0;
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
                for (; i < size; ++i) {
                    R result = body(it[i], end_it);
                    if (result.has_value())
                        return result;
                }
                return R{};
            } else {
                std::size_t i = 0;
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
                for (; i < size; ++i) {
                    R result = body(it[i], end_it);
                    if (result != end_it)
                        return result;
                }
                return static_cast<R>(end_it);
            }
        }

    } // namespace detail

    template<std::size_t N = 4, std::integral T, typename F>
        requires detail::ForUntypedCtrlBody<F, T>
    ForResult for_loop(T start, T end, F&& body) {
        return detail::for_loop_untyped_impl<N>(start, end, std::forward<F>(body));
    }

    template<typename R, std::size_t N = 4, std::integral T, typename F>
        requires detail::ForTypedCtrlBody<F, T, R>
    ForResultTyped<R> for_loop_typed(T start, T end, F&& body) {
        return detail::for_loop_typed_impl<R, N>(start, end, std::forward<F>(body));
    }

    template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
        requires detail::ForRangeUntypedCtrlBody<F, std::ranges::range_reference_t<Range>>
    ForResult for_loop_range(Range&& range, F&& body) {
        return detail::for_loop_range_untyped_impl<N>(std::forward<Range>(range), std::forward<F>(body));
    }

    template<typename R, std::size_t N = 4, std::ranges::random_access_range Range, typename F>
        requires detail::ForRangeTypedCtrlBody<F, std::ranges::range_reference_t<Range>, R>
    ForResultTyped<R> for_loop_range_typed(Range&& range, F&& body) {
        return detail::for_loop_range_typed_impl<R, N>(std::forward<Range>(range), std::forward<F>(body));
    }

    template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
    auto for_loop_range_ret_simple(Range&& range, F&& body) {
        return detail::for_loop_range_ret_simple_impl<N>(std::forward<Range>(range), std::forward<F>(body));
    }

    template<LoopType LT, std::integral T, typename F>
        requires detail::ForUntypedCtrlBody<F, T>
    ForResult for_loop_auto(T start, T end, F&& body) {
        return for_loop<optimal_N<LT, T>>(start, end, std::forward<F>(body));
    }

    template<typename R, LoopType LT, std::integral T, typename F>
        requires detail::ForTypedCtrlBody<F, T, R>
    ForResultTyped<R> for_loop_typed_auto(T start, T end, F&& body) {
        return for_loop_typed<R, optimal_N<LT, T>>(start, end, std::forward<F>(body));
    }

    template<LoopType LT, std::ranges::random_access_range Range, typename F>
        requires detail::ForRangeUntypedCtrlBody<F, std::ranges::range_reference_t<Range>>
    ForResult for_loop_range_auto(Range&& range, F&& body) {
        using T = std::ranges::range_value_t<Range>;
        return for_loop_range<optimal_N<LT, T>>(std::forward<Range>(range), std::forward<F>(body));
    }

    template<typename R, LoopType LT, std::ranges::random_access_range Range, typename F>
        requires detail::ForRangeTypedCtrlBody<F, std::ranges::range_reference_t<Range>, R>
    ForResultTyped<R> for_loop_range_typed_auto(Range&& range, F&& body) {
        using T = std::ranges::range_value_t<Range>;
        return for_loop_range_typed<R, optimal_N<LT, T>>(std::forward<Range>(range), std::forward<F>(body));
    }

} // namespace ilp
