#pragma once

// Pragma loop implementations - uses #pragma unroll for GCC auto-vectorization
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

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_simple_impl(T start, T end, F&& body) {
    _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
    for (T i = start; i < end; ++i) {
        body(i);
    }
}

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T>
auto for_loop_ret_simple_impl(T start, T end, F&& body) {
    using R = std::invoke_result_t<F, T>;

    if constexpr (std::is_same_v<R, bool>) {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (T i = start; i < end; ++i) {
            if (body(i)) return i;
        }
        return end;
    } else if constexpr (is_optional_v<R>) {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (T i = start; i < end; ++i) {
            if (auto r = body(i)) return r;
        }
        return R{};
    } else {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (T i = start; i < end; ++i) {
            if (auto r = body(i); r != end) return r;
        }
        return static_cast<R>(end);
    }
}

// =============================================================================
// Step loops
// =============================================================================

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_step_simple_impl(T start, T end, T step, F&& body) {
    if (step > 0) {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
        for (T i = start; i < end; i += step) body(i);
    } else {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
        for (T i = start; i > end; i += step) body(i);
    }
}

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T>
auto for_loop_step_ret_simple_impl(T start, T end, T step, F&& body) {
    using R = std::invoke_result_t<F, T>;

    if constexpr (std::is_same_v<R, bool>) {
        if (step > 0) {
            _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
            for (T i = start; i < end; i += step) {
                if (body(i)) return i;
            }
        } else {
            _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
            for (T i = start; i > end; i += step) {
                if (body(i)) return i;
            }
        }
        return end;
    } else if constexpr (is_optional_v<R>) {
        if (step > 0) {
            _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
            for (T i = start; i < end; i += step) {
                if (auto r = body(i)) return r;
            }
        } else {
            _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
            for (T i = start; i > end; i += step) {
                if (auto r = body(i)) return r;
            }
        }
        return R{};
    } else {
        if (step > 0) {
            _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
            for (T i = start; i < end; i += step) {
                if (auto r = body(i); r != end) return r;
            }
        } else {
            _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
            for (T i = start; i > end; i += step) {
                if (auto r = body(i); r != end) return r;
            }
        }
        return static_cast<R>(end);
    }
}

// =============================================================================
// Range-based loops
// =============================================================================

template<std::size_t N, std::ranges::random_access_range Range, typename F>
void for_loop_range_simple_impl(Range&& range, F&& body) {
    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);
    _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
    for (std::size_t i = 0; i < size; ++i) {
        body(it[i]);
    }
}

template<std::size_t N, std::ranges::random_access_range Range, typename F>
auto for_loop_range_ret_simple_impl(Range&& range, F&& body) {
    auto it = std::ranges::begin(range);
    auto end_it = std::ranges::end(range);
    auto size = std::ranges::size(range);
    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>>;

    if constexpr (std::is_same_v<R, bool>) {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (std::size_t i = 0; i < size; ++i) {
            if (body(it[i])) return it + i;
        }
        return end_it;
    } else if constexpr (is_optional_v<R>) {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (std::size_t i = 0; i < size; ++i) {
            if (auto r = body(it[i])) return r;
        }
        return R{};
    } else {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (std::size_t i = 0; i < size; ++i) {
            if (auto r = body(it[i]); r != end_it) return r;
        }
        return static_cast<R>(end_it);
    }
}

template<std::size_t N, std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, std::size_t>
auto for_loop_range_idx_ret_simple_impl(Range&& range, F&& body) {
    auto it = std::ranges::begin(range);
    auto end_it = std::ranges::end(range);
    auto size = std::ranges::size(range);
    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>, std::size_t>;

    if constexpr (std::is_same_v<R, bool>) {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (std::size_t i = 0; i < size; ++i) {
            if (body(it[i], i)) return it + i;
        }
        return end_it;
    } else if constexpr (is_optional_v<R>) {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (std::size_t i = 0; i < size; ++i) {
            if (auto r = body(it[i], i)) return r;
        }
        return R{};
    } else {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
        for (std::size_t i = 0; i < size; ++i) {
            if (auto r = body(it[i], i); r != end_it) return r;
        }
        return static_cast<R>(end_it);
    }
}

// =============================================================================
// Reduce implementations
// =============================================================================

template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_simple_impl(T start, T end, Init init, BinaryOp op, F&& body) {
    auto acc = init;
    _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
    for (T i = start; i < end; ++i) {
        acc = op(acc, body(i));
    }
    return acc;
}

template<std::size_t N, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_simple_impl(Range&& range, Init init, BinaryOp op, F&& body) {
    auto acc = init;
    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);
    _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
    for (std::size_t i = 0; i < size; ++i) {
        acc = op(acc, body(it[i]));
    }
    return acc;
}

template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_step_simple_impl(T start, T end, T step, Init init, BinaryOp op, F&& body) {
    auto acc = init;
    if (step > 0) {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
        for (T i = start; i < end; i += step) acc = op(acc, body(i));
    } else {
        _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
        for (T i = start; i > end; i += step) acc = op(acc, body(i));
    }
    return acc;
}

// =============================================================================
// Reduce with LoopCtrl (break support)
// =============================================================================

template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
auto reduce_impl(T start, T end, Init init, BinaryOp op, F&& body) {
    auto acc = init;
    LoopCtrl<void> ctrl;
    _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
    for (T i = start; i < end && ctrl.ok; ++i) {
        acc = op(acc, body(i, ctrl));
    }
    return acc;
}

template<std::size_t N, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, LoopCtrl<void>&>
auto reduce_range_impl(Range&& range, Init init, BinaryOp op, F&& body) {
    auto acc = init;
    LoopCtrl<void> ctrl;
    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);
    _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SUM_4))
    for (std::size_t i = 0; i < size && ctrl.ok; ++i) {
        acc = op(acc, body(it[i], ctrl));
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
    _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
    for (T i = start; i < end && ctrl.ok; ++i) {
        body(i, ctrl);
    }
    return ctrl.return_value;
}

template<typename R, std::size_t N, std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, LoopCtrl<R>&>
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
// Public API - Simple variants only
// =============================================================================

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_simple(T start, T end, F&& body) {
    detail::for_loop_simple_impl<N>(start, end, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
auto for_loop_ret_simple(T start, T end, F&& body) {
    return detail::for_loop_ret_simple_impl<N>(start, end, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_step_simple(T start, T end, T step, F&& body) {
    detail::for_loop_step_simple_impl<N>(start, end, step, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
auto for_loop_step_ret_simple(T start, T end, T step, F&& body) {
    return detail::for_loop_step_ret_simple_impl<N>(start, end, step, std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
void for_loop_range_simple(Range&& range, F&& body) {
    detail::for_loop_range_simple_impl<N>(std::forward<Range>(range), std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
auto for_loop_range_ret_simple(Range&& range, F&& body) {
    return detail::for_loop_range_ret_simple_impl<N>(std::forward<Range>(range), std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
auto for_loop_range_idx_ret_simple(Range&& range, F&& body) {
    return detail::for_loop_range_idx_ret_simple_impl<N>(std::forward<Range>(range), std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_simple(T start, T end, Init init, BinaryOp op, F&& body) {
    return detail::reduce_simple_impl<N>(start, end, init, op, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
auto reduce_sum(T start, T end, F&& body) {
    using R = std::invoke_result_t<F, T>;
    return detail::reduce_simple_impl<N>(start, end, R{}, std::plus<>{}, std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_simple(Range&& range, Init init, BinaryOp op, F&& body) {
    return detail::reduce_range_simple_impl<N>(std::forward<Range>(range), init, op, std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_sum(Range&& range, F&& body) {
    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>>;
    return detail::reduce_range_simple_impl<N>(std::forward<Range>(range), R{}, std::plus<>{}, std::forward<F>(body));
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
    requires std::invocable<F, T>
auto for_loop_ret_simple_auto(T start, T end, F&& body) {
    return detail::for_loop_ret_simple_impl<optimal_N<LoopType::Search, 4>>(start, end, std::forward<F>(body));
}

template<std::integral T, typename F>
    requires std::invocable<F, T>
auto reduce_sum_auto(T start, T end, F&& body) {
    return reduce_sum<optimal_N<LoopType::Sum, sizeof(T)>>(start, end, std::forward<F>(body));
}

template<std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_simple_auto(T start, T end, Init init, BinaryOp op, F&& body) {
    return reduce_simple<optimal_N<LoopType::Sum, sizeof(T)>>(start, end, init, op, std::forward<F>(body));
}

template<std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_sum_auto(Range&& range, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    return reduce_range_sum<optimal_N<LoopType::Sum, sizeof(T)>>(std::forward<Range>(range), std::forward<F>(body));
}

template<std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_simple_auto(Range&& range, Init init, BinaryOp op, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    return reduce_range_simple<optimal_N<LoopType::Sum, sizeof(T)>>(std::forward<Range>(range), init, op, std::forward<F>(body));
}

template<std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, std::size_t>
auto for_loop_range_idx_ret_simple_auto(Range&& range, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    return for_loop_range_idx_ret_simple<optimal_N<LoopType::Search, sizeof(T)>>(std::forward<Range>(range), std::forward<F>(body));
}

} // namespace ilp
