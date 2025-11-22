#pragma once

#include <cstddef>
#include <optional>
#include <utility>
#include <concepts>
#include <ranges>

#include "ctrl.hpp"

namespace ilp {
namespace detail {

// =============================================================================
// Index-based loops
// =============================================================================

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_simple_impl(T start, T end, F&& body) {
    T i = start;

    for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (body(i + static_cast<T>(Is)), ...);
        }(std::make_index_sequence<N>{});
    }

    for (; i < end; ++i) {
        body(i);
    }
}

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
void for_loop_impl(T start, T end, F&& body) {
    LoopCtrl<void> ctrl;
    T i = start;

    for (; i + static_cast<T>(N) <= end && ctrl.ok; i += static_cast<T>(N)) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] { body(i + static_cast<T>(Is), ctrl); return ctrl.ok; }() && ...);
        }(std::make_index_sequence<N>{});
    }

    for (; i < end && ctrl.ok; ++i) {
        body(i, ctrl);
    }
}

template<typename R, std::size_t N, std::integral T, typename F>
std::optional<R> for_loop_ret_impl(T start, T end, F&& body) {
    LoopCtrl<R> ctrl;
    T i = start;

    for (; i + static_cast<T>(N) <= end && ctrl.ok; i += static_cast<T>(N)) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] { body(i + static_cast<T>(Is), ctrl); return ctrl.ok; }() && ...);
        }(std::make_index_sequence<N>{});
    }

    for (; i < end && ctrl.ok; ++i) {
        body(i, ctrl);
    }

    return std::move(ctrl.return_value);
}

// =============================================================================
// Step loops
// =============================================================================

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_step_simple_impl(T start, T end, T step, F&& body) {
    T i = start;
    T last_offset = step * static_cast<T>(N - 1);

    auto in_range = [&](T val) {
        return step > 0 ? val < end : val > end;
    };

    while (in_range(i) && in_range(i + last_offset)) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (body(i + step * static_cast<T>(Is)), ...);
        }(std::make_index_sequence<N>{});
        i += step * static_cast<T>(N);
    }

    while (in_range(i)) {
        body(i);
        i += step;
    }
}

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
void for_loop_step_impl(T start, T end, T step, F&& body) {
    LoopCtrl<void> ctrl;
    T i = start;
    T last_offset = step * static_cast<T>(N - 1);

    auto in_range = [&](T val) {
        return step > 0 ? val < end : val > end;
    };

    while (in_range(i) && in_range(i + last_offset) && ctrl.ok) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] { body(i + step * static_cast<T>(Is), ctrl); return ctrl.ok; }() && ...);
        }(std::make_index_sequence<N>{});
        i += step * static_cast<T>(N);
    }

    while (in_range(i) && ctrl.ok) {
        body(i, ctrl);
        i += step;
    }
}

template<typename R, std::size_t N, std::integral T, typename F>
std::optional<R> for_loop_step_ret_impl(T start, T end, T step, F&& body) {
    LoopCtrl<R> ctrl;
    T i = start;
    T last_offset = step * static_cast<T>(N - 1);

    auto in_range = [&](T val) {
        return step > 0 ? val < end : val > end;
    };

    while (in_range(i) && in_range(i + last_offset) && ctrl.ok) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] { body(i + step * static_cast<T>(Is), ctrl); return ctrl.ok; }() && ...);
        }(std::make_index_sequence<N>{});
        i += step * static_cast<T>(N);
    }

    while (in_range(i) && ctrl.ok) {
        body(i, ctrl);
        i += step;
    }

    return std::move(ctrl.return_value);
}

// =============================================================================
// Range-based loops
// =============================================================================

template<std::size_t N, std::ranges::random_access_range Range, typename F>
void for_loop_range_simple_impl(Range&& range, F&& body) {
    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);
    std::size_t i = 0;

    for (; i + N <= size; i += N) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (body(it[i + Is]), ...);
        }(std::make_index_sequence<N>{});
    }

    for (; i < size; ++i) {
        body(it[i]);
    }
}

template<std::size_t N, std::ranges::random_access_range Range, typename F>
void for_loop_range_impl(Range&& range, F&& body) {
    LoopCtrl<void> ctrl;
    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);
    std::size_t i = 0;

    for (; i + N <= size && ctrl.ok; i += N) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] { body(it[i + Is], ctrl); return ctrl.ok; }() && ...);
        }(std::make_index_sequence<N>{});
    }

    for (; i < size && ctrl.ok; ++i) {
        body(it[i], ctrl);
    }
}

template<typename R, std::size_t N, std::ranges::random_access_range Range, typename F>
std::optional<R> for_loop_range_ret_impl(Range&& range, F&& body) {
    LoopCtrl<R> ctrl;
    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);
    std::size_t i = 0;

    for (; i + N <= size && ctrl.ok; i += N) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] { body(it[i + Is], ctrl); return ctrl.ok; }() && ...);
        }(std::make_index_sequence<N>{});
    }

    for (; i < size && ctrl.ok; ++i) {
        body(it[i], ctrl);
    }

    return std::move(ctrl.return_value);
}

} // namespace detail
} // namespace ilp
