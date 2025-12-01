#pragma once

// Simple loop implementations - plain sequential loops for testing/debugging
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
        for (T i = start; i < end && ctrl.ok; ++i) {
            body(i, ctrl);
        }
    } else {
        static_assert(ForBody<F, T>, "Lambda must be invocable with (T) or (T, LoopCtrl<void>&)");
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
        for (T i = start; i < end; ++i) {
            if (body(i, end)) return i;
        }
        return end;
    } else if constexpr (is_optional_v<R>) {
        for (T i = start; i < end; ++i) {
            if (auto r = body(i, end)) return r;
        }
        return R{};
    } else {
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

    if constexpr (has_ctrl) {
        LoopCtrl<void> ctrl;
        for (auto&& elem : range) {
            if (!ctrl.ok) break;
            body(elem, ctrl);
        }
    } else {
        static_assert(ForRangeBody<F, Ref>, "Lambda must be invocable with (Ref) or (Ref, LoopCtrl<void>&)");
        for (auto&& elem : range) {
            body(elem);
        }
    }
}

template<std::size_t N, std::ranges::random_access_range Range, typename F>
auto for_loop_range_ret_simple_impl(Range&& range, F&& body) {
    auto it = std::ranges::begin(range);
    auto end_it = std::ranges::end(range);
    using Sentinel = decltype(end_it);
    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>, Sentinel>;

    if constexpr (std::is_same_v<R, bool>) {
        for (; it != end_it; ++it) {
            if (body(*it, end_it)) return it;
        }
        return end_it;
    } else if constexpr (is_optional_v<R>) {
        for (; it != end_it; ++it) {
            if (auto r = body(*it, end_it)) return r;
        }
        return R{};
    } else {
        for (; it != end_it; ++it) {
            if (auto r = body(*it, end_it); r != end_it) return r;
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
        for (std::size_t i = 0; i < size; ++i) {
            if (body(it[i], i, end_it)) return it + i;
        }
        return end_it;
    } else if constexpr (is_optional_v<R>) {
        for (std::size_t i = 0; i < size; ++i) {
            if (auto r = body(it[i], i, end_it)) return r;
        }
        return R{};
    } else {
        for (std::size_t i = 0; i < size; ++i) {
            if (auto r = body(it[i], i, end_it); r != end_it) return r;
        }
        return static_cast<R>(end_it);
    }
}

// Range-based find with simple bool predicate - returns iterator
template<std::size_t N, std::ranges::random_access_range Range, typename Pred>
    requires std::invocable<Pred, std::ranges::range_reference_t<Range>>
          && std::same_as<std::invoke_result_t<Pred, std::ranges::range_reference_t<Range>>, bool>
auto find_range_impl(Range&& range, Pred&& pred) {
    auto it = std::ranges::begin(range);
    auto end_it = std::ranges::end(range);
    auto size = std::ranges::size(range);
    for (std::size_t i = 0; i < size; ++i) {
        if (pred(it[i])) return it + static_cast<std::ptrdiff_t>(i);
    }
    return end_it;  // Not found sentinel
}

// =============================================================================
// Reduce implementations
// =============================================================================

// Unified reduce implementation - handles both simple (no ctrl) and ctrl-enabled lambdas
template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires ReduceBody<F, T> || ReduceCtrlBody<F, T>
auto reduce_impl(T start, T end, Init&& init, BinaryOp op, F&& body) {
    constexpr bool has_ctrl = ReduceCtrlBody<F, T>;

    if constexpr (has_ctrl) {
        using ResultT = std::invoke_result_t<F, T, LoopCtrl<void>&>;

        if constexpr (is_reduce_result_v<ResultT>) {
            // New API: ILP_REDUCE_RETURN/ILP_REDUCE_BREAK
            using R = decltype(std::declval<ResultT>().value);
            LoopCtrl<void> ctrl;
            R acc = std::forward<Init>(init);
            for (T i = start; i < end; ++i) {
                auto result = body(i, ctrl);
                if (result.did_break()) break;
                acc = op(std::move(acc), std::move(result.value));
            }
            return acc;
        } else {
            // Simple API: plain return value (no break support)
            using R = ResultT;
            LoopCtrl<void> ctrl;
            R acc = std::forward<Init>(init);
            for (T i = start; i < end; ++i) {
                acc = op(std::move(acc), body(i, ctrl));
            }
            return acc;
        }
    } else {
        static_assert(ReduceBody<F, T>, "Lambda must be invocable with (T) or (T, LoopCtrl<void>&)");
        using R = std::invoke_result_t<F, T>;
        R acc = std::forward<Init>(init);
        for (T i = start; i < end; ++i) {
            acc = op(std::move(acc), body(i));
        }
        return acc;
    }
}

// Unified range-based reduce - handles both simple (no ctrl) and ctrl-enabled lambdas
template<std::size_t N, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
auto reduce_range_impl(Range&& range, Init&& init, BinaryOp op, F&& body) {
    using Ref = std::ranges::range_reference_t<Range>;
    constexpr bool has_ctrl = ReduceRangeCtrlBody<F, Ref>;

    if constexpr (has_ctrl) {
        using ResultT = std::invoke_result_t<F, Ref, LoopCtrl<void>&>;

        if constexpr (is_reduce_result_v<ResultT>) {
            // New API: ILP_REDUCE_RETURN/ILP_REDUCE_BREAK
            using R = decltype(std::declval<ResultT>().value);
            LoopCtrl<void> ctrl;
            R acc = std::forward<Init>(init);
            for (auto&& elem : range) {
                auto result = body(elem, ctrl);
                if (result.did_break()) break;
                acc = op(std::move(acc), std::move(result.value));
            }
            return acc;
        } else {
            // Simple API: plain return value (no break support)
            using R = ResultT;
            LoopCtrl<void> ctrl;
            R acc = std::forward<Init>(init);
            for (auto&& elem : range) {
                acc = op(std::move(acc), body(elem, ctrl));
            }
            return acc;
        }
    } else {
        // Dispatch to std::transform_reduce for SIMD vectorization
        return std::transform_reduce(
            std::ranges::begin(range),
            std::ranges::end(range),
            std::forward<Init>(init),
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
    for (T i = start; i < end && ctrl.ok; ++i) {
        body(i, ctrl);
    }
    return std::move(ctrl.return_value);
}

template<typename R, std::size_t N, std::ranges::random_access_range Range, typename F>
    requires ForRangeRetBody<F, std::ranges::range_reference_t<Range>, R>
std::optional<R> for_loop_range_ret_impl(Range&& range, F&& body) {
    LoopCtrl<R> ctrl;
    for (auto&& elem : range) {
        if (!ctrl.ok) break;
        body(elem, ctrl);
    }
    return std::move(ctrl.return_value);
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
auto reduce(T start, T end, Init&& init, BinaryOp op, F&& body) {
    return detail::reduce_impl<N>(start, end, std::forward<Init>(init), op, std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires detail::ReduceRangeBody<F, std::ranges::range_reference_t<Range>>
          || detail::ReduceRangeCtrlBody<F, std::ranges::range_reference_t<Range>>
auto reduce_range(Range&& range, Init&& init, BinaryOp op, F&& body) {
    return detail::reduce_range_impl<N>(std::forward<Range>(range), std::forward<Init>(init), op, std::forward<F>(body));
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

template<std::integral T, typename Init, typename BinaryOp, typename F>
    requires detail::ReduceBody<F, T> || detail::ReduceCtrlBody<F, T>
auto reduce_auto(T start, T end, Init&& init, BinaryOp op, F&& body) {
    return reduce<optimal_N<LoopType::Sum, T>>(start, end, std::forward<Init>(init), op, std::forward<F>(body));
}

template<std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires detail::ReduceRangeBody<F, std::ranges::range_reference_t<Range>>
          || detail::ReduceRangeCtrlBody<F, std::ranges::range_reference_t<Range>>
auto reduce_range_auto(Range&& range, Init&& init, BinaryOp op, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    return reduce_range<optimal_N<LoopType::Sum, T>>(std::forward<Range>(range), std::forward<Init>(init), op, std::forward<F>(body));
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
    for (T i = start; i < end; ++i) {
        if (pred(i)) return i;
    }
    return std::nullopt;
}

template<std::size_t N = 8, std::ranges::random_access_range Range, typename Pred>
std::optional<std::size_t> for_until_range(Range&& range, Pred&& pred) {
    auto it = std::ranges::begin(range);
    std::size_t n = std::ranges::size(range);
    for (std::size_t i = 0; i < n; ++i) {
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
