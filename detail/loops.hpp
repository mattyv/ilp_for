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

template<typename R, std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T>
std::optional<R> for_loop_ret_simple_impl(T start, T end, F&& body) {
    T i = start;

    for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
        std::optional<R> result;
        bool done = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return (([&] {
                result = body(i + static_cast<T>(Is));
                return result.has_value();
            }()) || ...);
        }(std::make_index_sequence<N>{});
        if (done) return result;
    }

    for (; i < end; ++i) {
        if (auto result = body(i)) return result;
    }

    return std::nullopt;
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

template<typename R, std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T>
std::optional<R> for_loop_step_ret_simple_impl(T start, T end, T step, F&& body) {
    T i = start;
    T last_offset = step * static_cast<T>(N - 1);

    auto in_range = [&](T val) {
        return step > 0 ? val < end : val > end;
    };

    while (in_range(i) && in_range(i + last_offset)) {
        std::optional<R> result;
        bool done = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return (([&] {
                result = body(i + step * static_cast<T>(Is));
                return result.has_value();
            }()) || ...);
        }(std::make_index_sequence<N>{});
        if (done) return result;
        i += step * static_cast<T>(N);
    }

    while (in_range(i)) {
        if (auto result = body(i)) return result;
        i += step;
    }

    return std::nullopt;
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

template<typename R, std::size_t N, std::ranges::random_access_range Range, typename F>
std::optional<R> for_loop_range_ret_simple_impl(Range&& range, F&& body) {
    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);
    std::size_t i = 0;

    for (; i + N <= size; i += N) {
        std::optional<R> result;
        bool done = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return (([&] {
                result = body(it[i + Is]);
                return result.has_value();
            }()) || ...);
        }(std::make_index_sequence<N>{});
        if (done) return result;
    }

    for (; i < size; ++i) {
        if (auto result = body(it[i])) return result;
    }

    return std::nullopt;
}

} // namespace detail
} // namespace ilp
