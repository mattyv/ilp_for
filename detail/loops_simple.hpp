#pragma once

// Simple loop implementations - plain sequential loops for testing/debugging
// Supports both _SIMPLE variants and LoopCtrl-based macros

#include <cstddef>
#include <functional>
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
void for_loop_impl(T start, T end, F&& body) {
    constexpr bool has_ctrl = std::invocable<F, T, LoopCtrl<void>&>;

    if constexpr (has_ctrl) {
        LoopCtrl<void> ctrl;
        for (T i = start; i < end && ctrl.ok; ++i) {
            body(i, ctrl);
        }
    } else {
        static_assert(std::invocable<F, T>, "Lambda must be invocable with (T) or (T, LoopCtrl<void>&)");
        for (T i = start; i < end; ++i) {
            body(i);
        }
    }
}

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T, T>
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
// Step loops
// =============================================================================

// Unified for_loop_step implementation - handles both simple (no ctrl) and ctrl-enabled lambdas
template<std::size_t N, std::integral T, typename F>
void for_loop_step_impl(T start, T end, T step, F&& body) {
    constexpr bool has_ctrl = std::invocable<F, T, LoopCtrl<void>&>;

    if constexpr (has_ctrl) {
        LoopCtrl<void> ctrl;
        if (step > 0) {
            for (T i = start; i < end && ctrl.ok; i += step) body(i, ctrl);
        } else {
            for (T i = start; i > end && ctrl.ok; i += step) body(i, ctrl);
        }
    } else {
        static_assert(std::invocable<F, T>, "Lambda must be invocable with (T) or (T, LoopCtrl<void>&)");
        if (step > 0) {
            for (T i = start; i < end; i += step) body(i);
        } else {
            for (T i = start; i > end; i += step) body(i);
        }
    }
}

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T, T>
auto for_loop_step_ret_simple_impl(T start, T end, T step, F&& body) {
    using R = std::invoke_result_t<F, T, T>;

    if constexpr (std::is_same_v<R, bool>) {
        if (step > 0) {
            for (T i = start; i < end; i += step) {
                if (body(i, end)) return i;
            }
        } else {
            for (T i = start; i > end; i += step) {
                if (body(i, end)) return i;
            }
        }
        return end;
    } else if constexpr (is_optional_v<R>) {
        if (step > 0) {
            for (T i = start; i < end; i += step) {
                if (auto r = body(i, end)) return r;
            }
        } else {
            for (T i = start; i > end; i += step) {
                if (auto r = body(i, end)) return r;
            }
        }
        return R{};
    } else {
        if (step > 0) {
            for (T i = start; i < end; i += step) {
                if (auto r = body(i, end); r != end) return r;
            }
        } else {
            for (T i = start; i > end; i += step) {
                if (auto r = body(i, end); r != end) return r;
            }
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
    constexpr bool has_ctrl = std::invocable<F, Ref, LoopCtrl<void>&>;

    if constexpr (has_ctrl) {
        LoopCtrl<void> ctrl;
        for (auto&& elem : range) {
            if (!ctrl.ok) break;
            body(elem, ctrl);
        }
    } else {
        static_assert(std::invocable<F, Ref>, "Lambda must be invocable with (Ref) or (Ref, LoopCtrl<void>&)");
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

// Range-based find with simple bool predicate - returns index
template<std::size_t N, std::ranges::random_access_range Range, typename Pred>
    requires std::invocable<Pred, std::ranges::range_reference_t<Range>>
          && std::same_as<std::invoke_result_t<Pred, std::ranges::range_reference_t<Range>>, bool>
std::size_t find_range_impl(Range&& range, Pred&& pred) {
    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);
    for (std::size_t i = 0; i < size; ++i) {
        if (pred(it[i])) return i;
    }
    return size;  // Not found sentinel
}

// =============================================================================
// Reduce implementations
// =============================================================================

// Unified reduce implementation - handles both simple (no ctrl) and ctrl-enabled lambdas
template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
auto reduce_impl(T start, T end, Init init, BinaryOp op, F&& body) {
    constexpr bool has_ctrl = std::invocable<F, T, LoopCtrl<void>&>;

    if constexpr (has_ctrl) {
        auto acc = init;
        LoopCtrl<void> ctrl;
        for (T i = start; i < end && ctrl.ok; ++i) {
            acc = op(acc, body(i, ctrl));
        }
        return acc;
    } else {
        static_assert(std::invocable<F, T>, "Lambda must be invocable with (T) or (T, LoopCtrl<void>&)");
        auto acc = init;
        for (T i = start; i < end; ++i) {
            acc = op(acc, body(i));
        }
        return acc;
    }
}

// Unified range-based reduce - handles both simple (no ctrl) and ctrl-enabled lambdas
template<std::size_t N, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
auto reduce_range_impl(Range&& range, Init init, BinaryOp op, F&& body) {
    using Ref = std::ranges::range_reference_t<Range>;
    constexpr bool has_ctrl = std::invocable<F, Ref, LoopCtrl<void>&>;

    if constexpr (has_ctrl) {
        auto acc = init;
        LoopCtrl<void> ctrl;
        for (auto&& elem : range) {
            if (!ctrl.ok) break;
            acc = op(acc, body(elem, ctrl));
        }
        return acc;
    } else {
        static_assert(std::invocable<F, Ref>, "Lambda must be invocable with (Ref) or (Ref, LoopCtrl<void>&)");
        auto acc = init;
        for (auto&& elem : range) {
            acc = op(acc, body(elem));
        }
        return acc;
    }
}

template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_step_simple_impl(T start, T end, T step, Init init, BinaryOp op, F&& body) {
    auto acc = init;
    if (step > 0) {
        for (T i = start; i < end; i += step) acc = op(acc, body(i));
    } else {
        for (T i = start; i > end; i += step) acc = op(acc, body(i));
    }
    return acc;
}

// =============================================================================
// For loops with LoopCtrl (return support)
// =============================================================================

template<typename R, std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T, LoopCtrl<R>&>
std::optional<R> for_loop_ret_impl(T start, T end, F&& body) {
    LoopCtrl<R> ctrl;
    for (T i = start; i < end && ctrl.ok; ++i) {
        body(i, ctrl);
    }
    return ctrl.return_value;
}

template<typename R, std::size_t N, std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, LoopCtrl<R>&>
std::optional<R> for_loop_range_ret_impl(Range&& range, F&& body) {
    LoopCtrl<R> ctrl;
    for (auto&& elem : range) {
        if (!ctrl.ok) break;
        body(elem, ctrl);
    }
    return ctrl.return_value;
}

} // namespace detail

// =============================================================================
// Public API - Simple variants only
// =============================================================================

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_simple(T start, T end, F&& body) {
    detail::for_loop_impl<N>(start, end, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T, T>
auto find(T start, T end, F&& body) {
    return detail::find_impl<N>(start, end, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_step_simple(T start, T end, T step, F&& body) {
    detail::for_loop_step_impl<N>(start, end, step, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T, T>
auto for_loop_step_ret_simple(T start, T end, T step, F&& body) {
    return detail::for_loop_step_ret_simple_impl<N>(start, end, step, std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
void for_loop_range_simple(Range&& range, F&& body) {
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

// Simple bool-predicate find - returns index (size() if not found)
template<std::size_t N = 4, std::ranges::random_access_range Range, typename Pred>
    requires std::invocable<Pred, std::ranges::range_reference_t<Range>>
          && std::same_as<std::invoke_result_t<Pred, std::ranges::range_reference_t<Range>>, bool>
std::size_t find_range(Range&& range, Pred&& pred) {
    return detail::find_range_impl<N>(std::forward<Range>(range), std::forward<Pred>(pred));
}

// Auto-selecting N based on CPU profile
template<std::ranges::random_access_range Range, typename Pred>
    requires std::invocable<Pred, std::ranges::range_reference_t<Range>>
          && std::same_as<std::invoke_result_t<Pred, std::ranges::range_reference_t<Range>>, bool>
std::size_t find_range_auto(Range&& range, Pred&& pred) {
    using T = std::ranges::range_value_t<Range>;
    return detail::find_range_impl<optimal_N<LoopType::Search, T>>(std::forward<Range>(range), std::forward<Pred>(pred));
}

template<std::size_t N = 4, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_simple(T start, T end, Init init, BinaryOp op, F&& body) {
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
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_simple(Range&& range, Init init, BinaryOp op, F&& body) {
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

template<std::size_t N = 4, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_step_simple(T start, T end, T step, Init init, BinaryOp op, F&& body) {
    return detail::reduce_step_simple_impl<N>(start, end, step, init, op, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
auto reduce_step_sum(T start, T end, T step, F&& body) {
    using R = std::invoke_result_t<F, T>;

    // Check for potential overflow
    detail::check_sum_overflow<R, T>();

    return detail::reduce_step_simple_impl<N>(start, end, step, R{}, std::plus<>{}, std::forward<F>(body));
}

// LoopCtrl-based functions (break/return support)
template<std::size_t N = 4, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
auto reduce(T start, T end, Init init, BinaryOp op, F&& body) {
    return detail::reduce_impl<N>(start, end, init, op, std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, LoopCtrl<void>&>
auto reduce_range(Range&& range, Init init, BinaryOp op, F&& body) {
    return detail::reduce_range_impl<N>(std::forward<Range>(range), init, op, std::forward<F>(body));
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
    requires std::invocable<F, T>
auto reduce_simple_auto(T start, T end, Init init, BinaryOp op, F&& body) {
    return reduce_simple<optimal_N<LoopType::Sum, T>>(start, end, init, op, std::forward<F>(body));
}

template<std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_sum_auto(Range&& range, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    return reduce_range_sum<optimal_N<LoopType::Sum, T>>(std::forward<Range>(range), std::forward<F>(body));
}

template<std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_simple_auto(Range&& range, Init init, BinaryOp op, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    return reduce_range_simple<optimal_N<LoopType::Sum, T>>(std::forward<Range>(range), init, op, std::forward<F>(body));
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
