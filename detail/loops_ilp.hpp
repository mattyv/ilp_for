#pragma once

// Full ILP loop implementations - multi-accumulator pattern for latency hiding
// Best for Clang which generates ccmp chains

#include <array>
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
    validate_unroll_factor<N>();
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
    validate_unroll_factor<N>();
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
    requires std::invocable<F, T, LoopCtrl<R>&>
std::optional<R> for_loop_ret_impl(T start, T end, F&& body) {
    validate_unroll_factor<N>();
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

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T>
auto for_loop_ret_simple_impl(T start, T end, F&& body) {
    validate_unroll_factor<N>();
    using R = std::invoke_result_t<F, T>;

    if constexpr (std::is_same_v<R, bool>) {
        // Bool mode - optimized for find, returns index
        T i = start;
        for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
            std::array<bool, N> matches;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((matches[Is] = body(i + static_cast<T>(Is))), ...);
            }(std::make_index_sequence<N>{});

            for (std::size_t j = 0; j < N; ++j) {
                if (matches[j]) return i + static_cast<T>(j);
            }
        }
        for (; i < end; ++i) {
            if (body(i)) return i;
        }
        return end;
    } else if constexpr (is_optional_v<R>) {
        // Optional mode - return first with value
        T i = start;
        for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
            std::array<R, N> results;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((results[Is] = body(i + static_cast<T>(Is))), ...);
            }(std::make_index_sequence<N>{});

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j].has_value()) return results[j];
            }
        }
        for (; i < end; ++i) {
            R result = body(i);
            if (result.has_value()) return result;
        }
        return R{};
    } else {
        // Value mode with sentinel - returns first != end
        T i = start;
        for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
            std::array<R, N> results;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((results[Is] = body(i + static_cast<T>(Is))), ...);
            }(std::make_index_sequence<N>{});

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j] != end) return results[j];
            }
        }
        for (; i < end; ++i) {
            R result = body(i);
            if (result != end) return result;
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
    validate_unroll_factor<N>();
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
    validate_unroll_factor<N>();
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
    requires std::invocable<F, T, LoopCtrl<R>&>
std::optional<R> for_loop_step_ret_impl(T start, T end, T step, F&& body) {
    validate_unroll_factor<N>();
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

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T>
auto for_loop_step_ret_simple_impl(T start, T end, T step, F&& body) {
    validate_unroll_factor<N>();
    using R = std::invoke_result_t<F, T>;
    T last_offset = step * static_cast<T>(N - 1);

    auto in_range = [&](T val) {
        return step > 0 ? val < end : val > end;
    };

    if constexpr (std::is_same_v<R, bool>) {
        // Bool mode - optimized for find, returns index
        T i = start;
        while (in_range(i) && in_range(i + last_offset)) {
            std::array<bool, N> matches;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((matches[Is] = body(i + step * static_cast<T>(Is))), ...);
            }(std::make_index_sequence<N>{});

            for (std::size_t j = 0; j < N; ++j) {
                if (matches[j]) return i + step * static_cast<T>(j);
            }
            i += step * static_cast<T>(N);
        }
        while (in_range(i)) {
            if (body(i)) return i;
            i += step;
        }
        return end;
    } else if constexpr (is_optional_v<R>) {
        // Optional mode - return first with value
        T i = start;
        while (in_range(i) && in_range(i + last_offset)) {
            std::array<R, N> results;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((results[Is] = body(i + step * static_cast<T>(Is))), ...);
            }(std::make_index_sequence<N>{});

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j].has_value()) return results[j];
            }
            i += step * static_cast<T>(N);
        }
        while (in_range(i)) {
            R result = body(i);
            if (result.has_value()) return result;
            i += step;
        }
        return R{};
    } else {
        // Value mode with sentinel
        T i = start;
        while (in_range(i) && in_range(i + last_offset)) {
            std::array<R, N> results;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((results[Is] = body(i + step * static_cast<T>(Is))), ...);
            }(std::make_index_sequence<N>{});

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j] != end) return results[j];
            }
            i += step * static_cast<T>(N);
        }
        while (in_range(i)) {
            R result = body(i);
            if (result != end) return result;
            i += step;
        }
        return static_cast<R>(end);
    }
}

// =============================================================================
// Range-based loops
// =============================================================================

template<std::size_t N, std::ranges::random_access_range Range, typename F>
void for_loop_range_simple_impl(Range&& range, F&& body) {
    validate_unroll_factor<N>();
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
    validate_unroll_factor<N>();
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
    validate_unroll_factor<N>();
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

template<std::size_t N, std::ranges::random_access_range Range, typename F>
auto for_loop_range_ret_simple_impl(Range&& range, F&& body) {
    validate_unroll_factor<N>();
    auto it = std::ranges::begin(range);
    auto end_it = std::ranges::end(range);
    auto size = std::ranges::size(range);

    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>>;

    if constexpr (std::is_same_v<R, bool>) {
        // Bool mode - return iterator to first match
        std::size_t i = 0;
        for (; i + N <= size; i += N) {
            std::array<bool, N> matches;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((matches[Is] = body(it[i + Is])), ...);
            }(std::make_index_sequence<N>{});

            for (std::size_t j = 0; j < N; ++j) {
                if (matches[j]) return it + (i + j);
            }
        }
        for (; i < size; ++i) {
            if (body(it[i])) return it + i;
        }
        return end_it;
    } else if constexpr (is_optional_v<R>) {
        // Optional mode - return first with value
        std::size_t i = 0;
        for (; i + N <= size; i += N) {
            std::array<R, N> results;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((results[Is] = body(it[i + Is])), ...);
            }(std::make_index_sequence<N>{});

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j].has_value()) return results[j];
            }
        }
        for (; i < size; ++i) {
            R result = body(it[i]);
            if (result.has_value()) return result;
        }
        return R{};
    } else {
        // Value mode with sentinel
        std::size_t i = 0;
        for (; i + N <= size; i += N) {
            std::array<R, N> results;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((results[Is] = body(it[i + Is])), ...);
            }(std::make_index_sequence<N>{});

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j] != end_it) return results[j];
            }
        }
        for (; i < size; ++i) {
            R result = body(it[i]);
            if (result != end_it) return result;
        }
        return static_cast<R>(end_it);
    }
}

// Range-based early-return with index (simple - no control flow)
template<std::size_t N, std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, std::size_t>
auto for_loop_range_idx_ret_simple_impl(Range&& range, F&& body) {
    validate_unroll_factor<N>();
    auto it = std::ranges::begin(range);
    auto end_it = std::ranges::end(range);
    auto size = std::ranges::size(range);

    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>, std::size_t>;

    if constexpr (std::is_same_v<R, bool>) {
        // Bool mode - return iterator to first match
        std::size_t i = 0;
        for (; i + N <= size; i += N) {
            std::array<bool, N> matches;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((matches[Is] = body(it[i + Is], i + Is)), ...);
            }(std::make_index_sequence<N>{});

            for (std::size_t j = 0; j < N; ++j) {
                if (matches[j]) return it + (i + j);
            }
        }
        for (; i < size; ++i) {
            if (body(it[i], i)) return it + i;
        }
        return end_it;
    } else if constexpr (is_optional_v<R>) {
        // Optional mode - return first with value
        std::size_t i = 0;
        for (; i + N <= size; i += N) {
            std::array<R, N> results;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((results[Is] = body(it[i + Is], i + Is)), ...);
            }(std::make_index_sequence<N>{});

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j].has_value()) return results[j];
            }
        }
        for (; i < size; ++i) {
            R result = body(it[i], i);
            if (result.has_value()) return result;
        }
        return R{};
    } else {
        // Value mode with sentinel
        std::size_t i = 0;
        for (; i + N <= size; i += N) {
            std::array<R, N> results;
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                ((results[Is] = body(it[i + Is], i + Is)), ...);
            }(std::make_index_sequence<N>{});

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j] != end_it) return results[j];
            }
        }
        for (; i < size; ++i) {
            R result = body(it[i], i);
            if (result != end_it) return result;
        }
        return static_cast<R>(end_it);
    }
}

// =============================================================================
// Reduce loops (multi-accumulator for true ILP)
// =============================================================================

template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
auto reduce_impl(T start, T end, Init init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    using R = std::invoke_result_t<F, T, LoopCtrl<void>&>;

    std::array<R, N> accs;
    accs.fill(init);

    LoopCtrl<void> ctrl;
    T i = start;

    // Main unrolled loop - each position feeds different accumulator
    for (; i + static_cast<T>(N) <= end && ctrl.ok; i += static_cast<T>(N)) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (ctrl.ok) {
                    accs[Is] = op(accs[Is], body(i + static_cast<T>(Is), ctrl));
                }
                return ctrl.ok;
            }() && ...);
        }(std::make_index_sequence<N>{});
        if (!ctrl.ok) { break; }
    }

    // Remainder - all go to accumulator 0
    for (; i < end && ctrl.ok; ++i) {
        accs[0] = op(accs[0], body(i, ctrl));
    }

    // Final reduction (unrolled via fold expression)
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        R result = accs[0];
        ((result = op(result, accs[Is + 1])), ...);
        return result;
    }(std::make_index_sequence<N - 1>{});
}

// Simple version without break support
template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_simple_impl(T start, T end, Init init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    using R = std::invoke_result_t<F, T>;

    std::array<R, N> accs;
    accs.fill(init);

    T i = start;

    // Main unrolled loop
    for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ((accs[Is] = op(accs[Is], body(i + static_cast<T>(Is)))), ...);
        }(std::make_index_sequence<N>{});
    }

    // Remainder
    for (; i < end; ++i) {
        accs[0] = op(accs[0], body(i));
    }

    // Final reduction (unrolled via fold expression)
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        R result = accs[0];
        ((result = op(result, accs[Is + 1])), ...);
        return result;
    }(std::make_index_sequence<N - 1>{});
}

// Range-based reduce
template<std::size_t N, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, LoopCtrl<void>&>
auto reduce_range_impl(Range&& range, Init init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>, LoopCtrl<void>&>;

    std::array<R, N> accs;
    accs.fill(init);

    LoopCtrl<void> ctrl;
    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);
    std::size_t i = 0;

    // Main unrolled loop
    for (; i + N <= size && ctrl.ok; i += N) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (ctrl.ok) {
                    accs[Is] = op(accs[Is], body(it[i + Is], ctrl));
                }
                return ctrl.ok;
            }() && ...);
        }(std::make_index_sequence<N>{});
        if (!ctrl.ok) { break; }
    }

    // Remainder
    for (; i < size && ctrl.ok; ++i) {
        accs[0] = op(accs[0], body(it[i], ctrl));
    }

    // Final reduction (unrolled via fold expression)
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        R result = accs[0];
        ((result = op(result, accs[Is + 1])), ...);
        return result;
    }(std::make_index_sequence<N - 1>{});
}

// Simple range reduce - optimized for contiguous ranges
template<std::size_t N, std::ranges::contiguous_range Range, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_simple_impl(Range&& range, Init init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>>;

    std::array<R, N> accs;
    accs.fill(init);

    auto* ptr = std::ranges::data(range);
    auto size = std::ranges::size(range);
    std::size_t i = 0;

    // Main unrolled loop - nested loop pattern enables universal vectorization (GCC + Clang)
    for (; i + N <= size; i += N) {
        for (std::size_t j = 0; j < N; ++j) {
            accs[j] = op(accs[j], body(ptr[i + j]));
        }
    }

    // Remainder
    for (; i < size; ++i) {
        accs[0] = op(accs[0], body(ptr[i]));
    }

    // Final reduction (unrolled via fold expression)
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        R result = accs[0];
        ((result = op(result, accs[Is + 1])), ...);
        return result;
    }(std::make_index_sequence<N - 1>{});
}

// Simple range reduce - fallback for random access ranges
template<std::size_t N, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires (!std::ranges::contiguous_range<Range>) && std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_simple_impl(Range&& range, Init init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>>;

    std::array<R, N> accs;
    accs.fill(init);

    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);
    std::size_t i = 0;

    // Main unrolled loop - nested loop pattern enables universal vectorization (GCC + Clang)
    for (; i + N <= size; i += N) {
        for (std::size_t j = 0; j < N; ++j) {
            accs[j] = op(accs[j], body(it[i + j]));
        }
    }

    // Remainder
    for (; i < size; ++i) {
        accs[0] = op(accs[0], body(it[i]));
    }

    // Final reduction (unrolled via fold expression)
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        R result = accs[0];
        ((result = op(result, accs[Is + 1])), ...);
        return result;
    }(std::make_index_sequence<N - 1>{});
}

// Step-based reduce
template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_step_simple_impl(T start, T end, T step, Init init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    using R = std::invoke_result_t<F, T>;

    std::array<R, N> accs;
    accs.fill(init);

    T i = start;
    T last_offset = step * static_cast<T>(N - 1);

    auto in_range = [&](T val) {
        return step > 0 ? val < end : val > end;
    };

    // Main unrolled loop
    while (in_range(i) && in_range(i + last_offset)) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ((accs[Is] = op(accs[Is], body(i + step * static_cast<T>(Is)))), ...);
        }(std::make_index_sequence<N>{});
        i += step * static_cast<T>(N);
    }

    // Remainder
    while (in_range(i)) {
        accs[0] = op(accs[0], body(i));
        i += step;
    }

    // Final reduction (unrolled via fold expression)
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        R result = accs[0];
        ((result = op(result, accs[Is + 1])), ...);
        return result;
    }(std::make_index_sequence<N - 1>{});
}

} // namespace detail

// =============================================================================
// Public API - Index-based loops
// =============================================================================

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_simple(T start, T end, F&& body) {
    detail::for_loop_simple_impl<N>(start, end, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
void for_loop(T start, T end, F&& body) {
    detail::for_loop_impl<N>(start, end, std::forward<F>(body));
}

template<typename R, std::size_t N = 4, std::integral T, typename F>
std::optional<R> for_loop_ret(T start, T end, F&& body) {
    return detail::for_loop_ret_impl<R, N>(start, end, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
auto for_loop_ret_simple(T start, T end, F&& body) {
    return detail::for_loop_ret_simple_impl<N>(start, end, std::forward<F>(body));
}

// =============================================================================
// Public API - Step loops
// =============================================================================

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_step_simple(T start, T end, T step, F&& body) {
    detail::for_loop_step_simple_impl<N>(start, end, step, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
void for_loop_step(T start, T end, T step, F&& body) {
    detail::for_loop_step_impl<N>(start, end, step, std::forward<F>(body));
}

template<typename R, std::size_t N = 4, std::integral T, typename F>
std::optional<R> for_loop_step_ret(T start, T end, T step, F&& body) {
    return detail::for_loop_step_ret_impl<R, N>(start, end, step, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
auto for_loop_step_ret_simple(T start, T end, T step, F&& body) {
    return detail::for_loop_step_ret_simple_impl<N>(start, end, step, std::forward<F>(body));
}

// =============================================================================
// Public API - Range-based loops
// =============================================================================

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
void for_loop_range_simple(Range&& range, F&& body) {
    detail::for_loop_range_simple_impl<N>(std::forward<Range>(range), std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
void for_loop_range(Range&& range, F&& body) {
    detail::for_loop_range_impl<N>(std::forward<Range>(range), std::forward<F>(body));
}

template<typename R, std::size_t N = 4, std::ranges::random_access_range Range, typename F>
std::optional<R> for_loop_range_ret(Range&& range, F&& body) {
    return detail::for_loop_range_ret_impl<R, N>(std::forward<Range>(range), std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
auto for_loop_range_ret_simple(Range&& range, F&& body) {
    return detail::for_loop_range_ret_simple_impl<N>(std::forward<Range>(range), std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
auto for_loop_range_idx_ret_simple(Range&& range, F&& body) {
    return detail::for_loop_range_idx_ret_simple_impl<N>(std::forward<Range>(range), std::forward<F>(body));
}

// =============================================================================
// Public API - Reduce
// =============================================================================

template<std::size_t N = 4, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
auto reduce(T start, T end, Init init, BinaryOp op, F&& body) {
    return detail::reduce_impl<N>(start, end, init, op, std::forward<F>(body));
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
    requires std::invocable<F, std::ranges::range_reference_t<Range>, LoopCtrl<void>&>
auto reduce_range(Range&& range, Init init, BinaryOp op, F&& body) {
    return detail::reduce_range_impl<N>(std::forward<Range>(range), init, op, std::forward<F>(body));
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

// =============================================================================
// Auto-selecting functions
// =============================================================================

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
