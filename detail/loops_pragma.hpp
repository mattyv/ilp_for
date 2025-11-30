#pragma once

// Pragma loop implementations - uses #pragma unroll for GCC auto-vectorization
// Supports both _SIMPLE variants and LoopCtrl-based macros

#include <cstddef>
#include <functional>
#include <numeric>
#include <utility>
#include <concepts>
#include <ranges>

#include "loops_common.hpp"
#include "ctrl.hpp"

namespace ilp {
namespace detail {

// =============================================================================
// Index-based loops
// =============================================================================

// Unified for_loop implementation - handles both simple (no ctrl) and ctrl-enabled lambdas
template<std::size_t N, std::integral T, typename F>
    requires ForBody<F, T> || ForCtrlBody<F, T>
void for_loop_impl(T start, T end, F&& body) {
    constexpr bool has_ctrl = ForCtrlBody<F, T>;

    if constexpr (has_ctrl) {
        LoopCtrl<void> ctrl;
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
        for (T i = start; i < end && ctrl.ok; ++i) {
            body(i, ctrl);
        }
    } else {
        static_assert(ForBody<F, T>, "Lambda must be invocable with (T) or (T, LoopCtrl<void>&)");
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
        for (T i = start; i < end; ++i) {
            body(i);
        }
    }
}

template<std::size_t N, std::integral T, typename F>
    requires FindBody<F, T>
auto find_impl(T start, T end, F&& body) {
    using R = std::invoke_result_t<F, T, T>;

    if constexpr (std::is_same_v<R, bool>) {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (T i = start; i < end; ++i) {
            if (body(i, end)) return i;
        }
        return end;
    } else if constexpr (is_optional_v<R>) {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (T i = start; i < end; ++i) {
            if (auto r = body(i, end)) return r;
        }
        return R{};
    } else {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (T i = start; i < end; ++i) {
            if (auto r = body(i, end); r != end) return r;
        }
        return static_cast<R>(end);
    }
}

// =============================================================================
// Range-based loops
// =============================================================================

// Unified for_loop_range implementation - handles both simple (no ctrl) and ctrl-enabled lambdas
template<std::size_t N, std::ranges::random_access_range Range, typename F>
void for_loop_range_impl(Range&& range, F&& body) {
    using Ref = std::ranges::range_reference_t<Range>;
    constexpr bool has_ctrl = ForRangeCtrlBody<F, Ref>;

    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);

    if constexpr (has_ctrl) {
        LoopCtrl<void> ctrl;
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
        for (std::size_t i = 0; i < size && ctrl.ok; ++i) {
            body(it[i], ctrl);
        }
    } else {
        static_assert(ForRangeBody<F, Ref>, "Lambda must be invocable with (Ref) or (Ref, LoopCtrl<void>&)");
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
        for (std::size_t i = 0; i < size; ++i) {
            body(it[i]);
        }
    }
}

template<std::size_t N, std::ranges::random_access_range Range, typename F>
auto for_loop_range_ret_simple_impl(Range&& range, F&& body) {
    auto it = std::ranges::begin(range);
    auto end_it = std::ranges::end(range);
    auto size = std::ranges::size(range);
    using Sentinel = decltype(end_it);
    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>, Sentinel>;

    if constexpr (std::is_same_v<R, bool>) {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (std::size_t i = 0; i < size; ++i) {
            if (body(it[i], end_it)) return it + i;
        }
        return end_it;
    } else if constexpr (is_optional_v<R>) {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (std::size_t i = 0; i < size; ++i) {
            if (auto r = body(it[i], end_it)) return r;
        }
        return R{};
    } else {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (std::size_t i = 0; i < size; ++i) {
            if (auto r = body(it[i], end_it); r != end_it) return r;
        }
        return static_cast<R>(end_it);
    }
}

template<std::size_t N, std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, std::size_t, decltype(std::ranges::end(std::declval<Range>()))>
auto find_range_idx_impl(Range&& range, F&& body) {
    auto it = std::ranges::begin(range);
    auto end_it = std::ranges::end(range);
    auto size = std::ranges::size(range);
    using Sentinel = decltype(end_it);
    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>, std::size_t, Sentinel>;

    if constexpr (std::is_same_v<R, bool>) {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (std::size_t i = 0; i < size; ++i) {
            if (body(it[i], i, end_it)) return it + i;
        }
        return end_it;
    } else if constexpr (is_optional_v<R>) {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (std::size_t i = 0; i < size; ++i) {
            if (auto r = body(it[i], i, end_it)) return r;
        }
        return R{};
    } else {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (std::size_t i = 0; i < size; ++i) {
            if (auto r = body(it[i], i, end_it); r != end_it) return r;
        }
        return static_cast<R>(end_it);
    }
}

// Range-based find with simple bool predicate - returns iterator
template<std::size_t N, std::ranges::random_access_range Range, typename Pred>
    requires PredicateRangeBody<Pred, std::ranges::range_reference_t<Range>>
auto find_range_impl(Range&& range, Pred&& pred) {
    auto it = std::ranges::begin(range);
    auto end_it = std::ranges::end(range);
    auto size = std::ranges::size(range);
    _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
    for (std::size_t i = 0; i < size; ++i) {
        if (pred(it[i])) return it + static_cast<std::ptrdiff_t>(i);
    }
    return end_it;  // Not found sentinel
}

// =============================================================================
// Reduce implementations (unified - auto-detect ctrl from signature)
// =============================================================================

template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires ReduceBody<F, T> || ReduceCtrlBody<F, T>
auto reduce_impl(T start, T end, Init init, BinaryOp op, F&& body) {
    constexpr bool has_ctrl = ReduceCtrlBody<F, T>;

    if constexpr (has_ctrl) {
        auto acc = init;
        LoopCtrl<void> ctrl;
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
        for (T i = start; i < end && ctrl.ok; ++i) {
            acc = op(acc, body(i, ctrl));
        }
        return acc;
    } else {
        static_assert(ReduceBody<F, T>, "Lambda must be invocable with (T) or (T, LoopCtrl<void>&)");
        auto acc = init;
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
        for (T i = start; i < end; ++i) {
            acc = op(acc, body(i));
        }
        return acc;
    }
}

template<std::size_t N, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
auto reduce_range_impl(Range&& range, Init init, BinaryOp op, F&& body) {
    using Ref = std::ranges::range_reference_t<Range>;
    constexpr bool has_ctrl = ReduceRangeCtrlBody<F, Ref>;

    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);

    if constexpr (has_ctrl) {
        auto acc = init;
        LoopCtrl<void> ctrl;
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
        for (std::size_t i = 0; i < size && ctrl.ok; ++i) {
            acc = op(acc, body(it[i], ctrl));
        }
        return acc;
    } else {
        // Dispatch to std::transform_reduce for SIMD vectorization
        return std::transform_reduce(
            std::ranges::begin(range),
            std::ranges::end(range),
            init,
            op,
            std::forward<F>(body)
        );
    }
}

// =============================================================================
// For loops with LoopCtrl (return support)
// =============================================================================

template<typename R, std::size_t N, std::integral T, typename F>
    requires ForRetBody<F, T, R>
std::optional<R> for_loop_ret_impl(T start, T end, F&& body) {
    LoopCtrl<R> ctrl;
    _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
    for (T i = start; i < end && ctrl.ok; ++i) {
        body(i, ctrl);
    }
    return ctrl.return_value;
}

template<typename R, std::size_t N, std::ranges::random_access_range Range, typename F>
    requires ForRangeRetBody<F, std::ranges::range_reference_t<Range>, R>
std::optional<R> for_loop_range_ret_impl(Range&& range, F&& body) {
    LoopCtrl<R> ctrl;
    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);
    _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
    for (std::size_t i = 0; i < size && ctrl.ok; ++i) {
        body(it[i], ctrl);
    }
    return ctrl.return_value;
}

} // namespace detail

// =============================================================================
// Public API - Index-based loops
// =============================================================================

template<std::size_t N = 4, std::integral T, typename F>
    requires detail::ForBody<F, T> || detail::ForCtrlBody<F, T>
void for_loop(T start, T end, F&& body) {
    detail::for_loop_impl<N>(start, end, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T, T>
auto find(T start, T end, F&& body) {
    return detail::find_impl<N>(start, end, std::forward<F>(body));
}

// =============================================================================
// Public API - Range-based loops
// =============================================================================

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
    requires detail::ForRangeBody<F, std::ranges::range_reference_t<Range>>
          || detail::ForRangeCtrlBody<F, std::ranges::range_reference_t<Range>>
void for_loop_range(Range&& range, F&& body) {
    detail::for_loop_range_impl<N>(std::forward<Range>(range), std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
auto for_loop_range_ret_simple(Range&& range, F&& body) {
    return detail::for_loop_range_ret_simple_impl<N>(std::forward<Range>(range), std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
auto find_range_idx(Range&& range, F&& body) {
    return detail::find_range_idx_impl<N>(std::forward<Range>(range), std::forward<F>(body));
}

// Simple bool-predicate find - returns iterator (end() if not found)
template<std::size_t N = 4, std::ranges::random_access_range Range, typename Pred>
    requires std::invocable<Pred, std::ranges::range_reference_t<Range>>
          && std::same_as<std::invoke_result_t<Pred, std::ranges::range_reference_t<Range>>, bool>
auto find_range(Range&& range, Pred&& pred) {
    return detail::find_range_impl<N>(std::forward<Range>(range), std::forward<Pred>(pred));
}

// Auto-selecting N based on CPU profile
template<std::ranges::random_access_range Range, typename Pred>
    requires std::invocable<Pred, std::ranges::range_reference_t<Range>>
          && std::same_as<std::invoke_result_t<Pred, std::ranges::range_reference_t<Range>>, bool>
auto find_range_auto(Range&& range, Pred&& pred) {
    using T = std::ranges::range_value_t<Range>;
    return detail::find_range_impl<optimal_N<LoopType::Search, T>>(std::forward<Range>(range), std::forward<Pred>(pred));
}

// =============================================================================
// Public API - Reduce
// =============================================================================

template<std::size_t N = 4, std::integral T, typename Init, typename BinaryOp, typename F>
    requires detail::ReduceBody<F, T> || detail::ReduceCtrlBody<F, T>
auto reduce(T start, T end, Init init, BinaryOp op, F&& body) {
    return detail::reduce_impl<N>(start, end, init, op, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
auto reduce_sum(T start, T end, F&& body) {
    using R = std::invoke_result_t<F, T>;

    // Check for potential overflow
    detail::check_sum_overflow<R, T>();

    return detail::reduce_impl<N>(start, end, R{}, std::plus<>{}, std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires detail::ReduceRangeBody<F, std::ranges::range_reference_t<Range>>
          || detail::ReduceRangeCtrlBody<F, std::ranges::range_reference_t<Range>>
auto reduce_range(Range&& range, Init init, BinaryOp op, F&& body) {
    return detail::reduce_range_impl<N>(std::forward<Range>(range), init, op, std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_sum(Range&& range, F&& body) {
    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>>;
    using ElemT = std::ranges::range_value_t<Range>;

    // Check for potential overflow
    detail::check_sum_overflow<R, ElemT>();

    return detail::reduce_range_impl<N>(std::forward<Range>(range), R{}, std::plus<>{}, std::forward<F>(body));
}

template<typename R, std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T, LoopCtrl<R>&>
std::optional<R> for_loop_ret(T start, T end, F&& body) {
    return detail::for_loop_ret_impl<R, N>(start, end, std::forward<F>(body));
}

template<typename R, std::size_t N = 4, std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, LoopCtrl<R>&>
std::optional<R> for_loop_range_ret(Range&& range, F&& body) {
    return detail::for_loop_range_ret_impl<R, N>(std::forward<Range>(range), std::forward<F>(body));
}

// Auto-selecting functions
template<std::integral T, typename F>
    requires std::invocable<F, T, T>
auto find_auto(T start, T end, F&& body) {
    return detail::find_impl<optimal_N<LoopType::Search, T>>(start, end, std::forward<F>(body));
}

template<std::integral T, typename F>
    requires std::invocable<F, T>
auto reduce_sum_auto(T start, T end, F&& body) {
    return reduce_sum<optimal_N<LoopType::Sum, T>>(start, end, std::forward<F>(body));
}

template<std::integral T, typename Init, typename BinaryOp, typename F>
    requires detail::ReduceBody<F, T> || detail::ReduceCtrlBody<F, T>
auto reduce_auto(T start, T end, Init init, BinaryOp op, F&& body) {
    return reduce<optimal_N<LoopType::Sum, T>>(start, end, init, op, std::forward<F>(body));
}

template<std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_sum_auto(Range&& range, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    return reduce_range_sum<optimal_N<LoopType::Sum, T>>(std::forward<Range>(range), std::forward<F>(body));
}

template<std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires detail::ReduceRangeBody<F, std::ranges::range_reference_t<Range>>
          || detail::ReduceRangeCtrlBody<F, std::ranges::range_reference_t<Range>>
auto reduce_range_auto(Range&& range, Init init, BinaryOp op, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    return reduce_range<optimal_N<LoopType::Sum, T>>(std::forward<Range>(range), init, op, std::forward<F>(body));
}

template<std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, std::size_t, decltype(std::ranges::end(std::declval<Range>()))>
auto find_range_idx_auto(Range&& range, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    return find_range_idx<optimal_N<LoopType::Search, T>>(std::forward<Range>(range), std::forward<F>(body));
}

// =============================================================================
// For-until loops (optimized early exit)
// =============================================================================

template<std::size_t N = 8, std::integral T, typename Pred>
    requires std::invocable<Pred, T> && std::same_as<std::invoke_result_t<Pred, T>, bool>
std::optional<T> for_until(T start, T end, Pred&& pred) {
    T i = start;
    _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
    for (; i < end; ++i) {
        if (pred(i)) return i;
    }
    return std::nullopt;
}

template<std::size_t N = 8, std::ranges::random_access_range Range, typename Pred>
std::optional<std::size_t> for_until_range(Range&& range, Pred&& pred) {
    auto it = std::ranges::begin(range);
    std::size_t n = std::ranges::size(range);
    std::size_t i = 0;
    _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
    for (; i < n; ++i) {
        if (pred(it[i])) return i;
    }
    return std::nullopt;
}

template<std::integral T, typename Pred>
    requires std::invocable<Pred, T> && std::same_as<std::invoke_result_t<Pred, T>, bool>
std::optional<T> for_until_auto(T start, T end, Pred&& pred) {
    return for_until<optimal_N<LoopType::Search, T>>(start, end, std::forward<Pred>(pred));
}

template<std::ranges::random_access_range Range, typename Pred>
std::optional<std::size_t> for_until_range_auto(Range&& range, Pred&& pred) {
    using T = std::ranges::range_value_t<Range>;
    return for_until_range<optimal_N<LoopType::Search, T>>(std::forward<Range>(range), std::forward<Pred>(pred));
}

} // namespace ilp
