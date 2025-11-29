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
#include "lambda_check.hpp"

namespace ilp {
namespace detail {

// =============================================================================
// Identity element inference for common operations
// =============================================================================

template<typename Op, typename T>
constexpr T operation_identity([[maybe_unused]] const Op& op, [[maybe_unused]] T init) {
    if constexpr (std::is_same_v<std::decay_t<Op>, std::plus<>> ||
                  std::is_same_v<std::decay_t<Op>, std::plus<T>>) {
        return T{};
    } else if constexpr (std::is_same_v<std::decay_t<Op>, std::multiplies<>> ||
                         std::is_same_v<std::decay_t<Op>, std::multiplies<T>>) {
        return T{1};
    } else if constexpr (std::is_same_v<std::decay_t<Op>, std::bit_and<>> ||
                         std::is_same_v<std::decay_t<Op>, std::bit_and<T>>) {
        return ~T{};  // All 1s
    } else if constexpr (std::is_same_v<std::decay_t<Op>, std::bit_or<>> ||
                         std::is_same_v<std::decay_t<Op>, std::bit_or<T>>) {
        return T{};
    } else if constexpr (std::is_same_v<std::decay_t<Op>, std::bit_xor<>> ||
                         std::is_same_v<std::decay_t<Op>, std::bit_xor<T>>) {
        return T{};
    } else {
        // For unknown operations (lambdas, etc.), fall back to using init as identity
        // This is only correct if init is actually the identity element
        return init;
    }
}

// =============================================================================
// Index-based loops
// =============================================================================

// Unified for_loop implementation - handles both simple (no ctrl) and ctrl-enabled lambdas
template<std::size_t N, std::integral T, typename F>
void for_loop_impl(T start, T end, F&& body) {
    validate_unroll_factor<N>();
    constexpr bool has_ctrl = std::invocable<F, T, LoopCtrl<void>&>;

    if constexpr (has_ctrl) {
        LoopCtrl<void> ctrl;
        T i = start;

        for (; i + static_cast<T>(N) <= end && ctrl.ok; i += static_cast<T>(N)) {
            for (std::size_t j = 0; j < N && ctrl.ok; ++j) {
                body(i + static_cast<T>(j), ctrl);
            }
        }

        for (; i < end && ctrl.ok; ++i) {
            body(i, ctrl);
        }
    } else {
        static_assert(std::invocable<F, T>, "Lambda must be invocable with (T) or (T, LoopCtrl<void>&)");
        T i = start;

        for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
            for (std::size_t j = 0; j < N; ++j) {
                body(i + static_cast<T>(j));
            }
        }

        for (; i < end; ++i) {
            body(i);
        }
    }
}

template<typename R, std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T, LoopCtrl<R>&>
std::optional<R> for_loop_ret_impl(T start, T end, F&& body) {
    validate_unroll_factor<N>();
    LoopCtrl<R> ctrl;
    T i = start;

    // Main unrolled loop - nested loop pattern enables universal vectorization (GCC + Clang)
    for (; i + static_cast<T>(N) <= end && ctrl.ok; i += static_cast<T>(N)) {
        for (std::size_t j = 0; j < N && ctrl.ok; ++j) {
            body(i + static_cast<T>(j), ctrl);
        }
    }

    for (; i < end && ctrl.ok; ++i) {
        body(i, ctrl);
    }

    return std::move(ctrl.return_value);
}

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T, T>
auto find_impl(T start, T end, F&& body) {
    validate_unroll_factor<N>();
    using R = std::invoke_result_t<F, T, T>;

    if constexpr (std::is_same_v<R, bool>) {
        // Bool mode - optimized for find, returns index
        T i = start;
        for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
            std::array<bool, N> matches;
            for (std::size_t j = 0; j < N; ++j) {
                matches[j] = body(i + static_cast<T>(j), end);
            }

            for (std::size_t j = 0; j < N; ++j) {
                if (matches[j]) return i + static_cast<T>(j);
            }
        }
        for (; i < end; ++i) {
            if (body(i, end)) return i;
        }
        return end;
    } else if constexpr (is_optional_v<R>) {
        // Optional mode - return first with value
        T i = start;
        for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
            std::array<R, N> results;
            for (std::size_t j = 0; j < N; ++j) {
                results[j] = body(i + static_cast<T>(j), end);
            }

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j].has_value()) return results[j];
            }
        }
        for (; i < end; ++i) {
            R result = body(i, end);
            if (result.has_value()) return result;
        }
        return R{};
    } else {
        // Value mode with sentinel - returns first != end
        T i = start;
        for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
            std::array<R, N> results;
            for (std::size_t j = 0; j < N; ++j) {
                results[j] = body(i + static_cast<T>(j), end);
            }

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j] != end) return results[j];
            }
        }
        for (; i < end; ++i) {
            R result = body(i, end);
            if (result != end) return result;
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
    validate_unroll_factor<N>();
    constexpr bool has_ctrl = std::invocable<F, T, LoopCtrl<void>&>;
    T i = start;
    T last_offset = step * static_cast<T>(N - 1);

    auto in_range = [&](T val) {
        return step > 0 ? val < end : val > end;
    };

    if constexpr (has_ctrl) {
        LoopCtrl<void> ctrl;

        while (in_range(i) && in_range(i + last_offset) && ctrl.ok) {
            for (std::size_t j = 0; j < N && ctrl.ok; ++j) {
                body(i + step * static_cast<T>(j), ctrl);
            }
            i += step * static_cast<T>(N);
        }

        while (in_range(i) && ctrl.ok) {
            body(i, ctrl);
            i += step;
        }
    } else {
        static_assert(std::invocable<F, T>, "Lambda must be invocable with (T) or (T, LoopCtrl<void>&)");

        while (in_range(i) && in_range(i + last_offset)) {
            for (std::size_t j = 0; j < N; ++j) {
                body(i + step * static_cast<T>(j));
            }
            i += step * static_cast<T>(N);
        }

        while (in_range(i)) {
            body(i);
            i += step;
        }
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
        for (std::size_t j = 0; j < N && ctrl.ok; ++j) {
            body(i + step * static_cast<T>(j), ctrl);
        }
        i += step * static_cast<T>(N);
    }

    while (in_range(i) && ctrl.ok) {
        body(i, ctrl);
        i += step;
    }

    return std::move(ctrl.return_value);
}

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T, T>
auto for_loop_step_ret_simple_impl(T start, T end, T step, F&& body) {
    validate_unroll_factor<N>();
    using R = std::invoke_result_t<F, T, T>;
    T last_offset = step * static_cast<T>(N - 1);

    auto in_range = [&](T val) {
        return step > 0 ? val < end : val > end;
    };

    if constexpr (std::is_same_v<R, bool>) {
        // Bool mode - optimized for find, returns index
        T i = start;
        while (in_range(i) && in_range(i + last_offset)) {
            std::array<bool, N> matches;
            for (std::size_t j = 0; j < N; ++j) {
                matches[j] = body(i + step * static_cast<T>(j), end);
            }

            for (std::size_t j = 0; j < N; ++j) {
                if (matches[j]) return i + step * static_cast<T>(j);
            }
            i += step * static_cast<T>(N);
        }
        while (in_range(i)) {
            if (body(i, end)) return i;
            i += step;
        }
        return end;
    } else if constexpr (is_optional_v<R>) {
        // Optional mode - return first with value
        T i = start;
        while (in_range(i) && in_range(i + last_offset)) {
            std::array<R, N> results;
            for (std::size_t j = 0; j < N; ++j) {
                results[j] = body(i + step * static_cast<T>(j), end);
            }

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j].has_value()) return results[j];
            }
            i += step * static_cast<T>(N);
        }
        while (in_range(i)) {
            R result = body(i, end);
            if (result.has_value()) return result;
            i += step;
        }
        return R{};
    } else {
        // Value mode with sentinel
        T i = start;
        while (in_range(i) && in_range(i + last_offset)) {
            std::array<R, N> results;
            for (std::size_t j = 0; j < N; ++j) {
                results[j] = body(i + step * static_cast<T>(j), end);
            }

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j] != end) return results[j];
            }
            i += step * static_cast<T>(N);
        }
        while (in_range(i)) {
            R result = body(i, end);
            if (result != end) return result;
            i += step;
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
    validate_unroll_factor<N>();
    using Ref = std::ranges::range_reference_t<Range>;
    constexpr bool has_ctrl = std::invocable<F, Ref, LoopCtrl<void>&>;

    // Check if lambda parameter is by-value (performance warning) - only for simple lambdas
    if constexpr (!has_ctrl) {
        check_range_lambda_param<F, Ref>();
    }

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
        static_assert(std::invocable<F, Ref>, "Lambda must be invocable with (Ref) or (Ref, LoopCtrl<void>&)");

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

template<typename R, std::size_t N, std::ranges::random_access_range Range, typename F>
std::optional<R> for_loop_range_ret_impl(Range&& range, F&& body) {
    validate_unroll_factor<N>();
    LoopCtrl<R> ctrl;
    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);
    std::size_t i = 0;

    // Main unrolled loop - nested loop pattern enables universal vectorization (GCC + Clang)
    for (; i + N <= size && ctrl.ok; i += N) {
        for (std::size_t j = 0; j < N && ctrl.ok; ++j) {
            body(it[i + j], ctrl);
        }
    }

    for (; i < size && ctrl.ok; ++i) {
        body(it[i], ctrl);
    }

    return std::move(ctrl.return_value);
}

template<std::size_t N, std::ranges::random_access_range Range, typename F>
auto for_loop_range_ret_simple_impl(Range&& range, F&& body) {
    validate_unroll_factor<N>();

    // Check if lambda parameter is by-value (performance warning)
    check_range_lambda_param<F, std::ranges::range_reference_t<Range>>();

    auto it = std::ranges::begin(range);
    auto end_it = std::ranges::end(range);
    auto size = std::ranges::size(range);
    using Sentinel = decltype(end_it);

    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>, Sentinel>;

    if constexpr (std::is_same_v<R, bool>) {
        // Bool mode - return iterator to first match
        std::size_t i = 0;
        for (; i + N <= size; i += N) {
            std::array<bool, N> matches;
            for (std::size_t j = 0; j < N; ++j) {
                matches[j] = body(it[i + j], end_it);
            }

            for (std::size_t j = 0; j < N; ++j) {
                if (matches[j]) return it + (i + j);
            }
        }
        for (; i < size; ++i) {
            if (body(it[i], end_it)) return it + i;
        }
        return end_it;
    } else if constexpr (is_optional_v<R>) {
        // Optional mode - return first with value
        std::size_t i = 0;
        for (; i + N <= size; i += N) {
            std::array<R, N> results;
            for (std::size_t j = 0; j < N; ++j) {
                results[j] = body(it[i + j], end_it);
            }

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j].has_value()) return results[j];
            }
        }
        for (; i < size; ++i) {
            R result = body(it[i], end_it);
            if (result.has_value()) return result;
        }
        return R{};
    } else {
        // Value mode with sentinel
        std::size_t i = 0;
        for (; i + N <= size; i += N) {
            std::array<R, N> results;
            for (std::size_t j = 0; j < N; ++j) {
                results[j] = body(it[i + j], end_it);
            }

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j] != end_it) return results[j];
            }
        }
        for (; i < size; ++i) {
            R result = body(it[i], end_it);
            if (result != end_it) return result;
        }
        return static_cast<R>(end_it);
    }
}

// Range-based early-return with index (simple - no control flow)
template<std::size_t N, std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, std::size_t, decltype(std::ranges::end(std::declval<Range>()))>
auto find_range_idx_impl(Range&& range, F&& body) {
    validate_unroll_factor<N>();

    // Check if lambda parameter is by-value (performance warning)
    check_range_lambda_param<F, std::ranges::range_reference_t<Range>>();

    auto it = std::ranges::begin(range);
    auto end_it = std::ranges::end(range);
    auto size = std::ranges::size(range);
    using Sentinel = decltype(end_it);

    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>, std::size_t, Sentinel>;

    if constexpr (std::is_same_v<R, bool>) {
        // Bool mode - return iterator to first match
        std::size_t i = 0;
        for (; i + N <= size; i += N) {
            std::array<bool, N> matches;
            for (std::size_t j = 0; j < N; ++j) {
                matches[j] = body(it[i + j], i + j, end_it);
            }

            for (std::size_t j = 0; j < N; ++j) {
                if (matches[j]) return it + (i + j);
            }
        }
        for (; i < size; ++i) {
            if (body(it[i], i, end_it)) return it + i;
        }
        return end_it;
    } else if constexpr (is_optional_v<R>) {
        // Optional mode - return first with value
        std::size_t i = 0;
        for (; i + N <= size; i += N) {
            std::array<R, N> results;
            for (std::size_t j = 0; j < N; ++j) {
                results[j] = body(it[i + j], i + j, end_it);
            }

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j].has_value()) return results[j];
            }
        }
        for (; i < size; ++i) {
            R result = body(it[i], i, end_it);
            if (result.has_value()) return result;
        }
        return R{};
    } else {
        // Value mode with sentinel
        std::size_t i = 0;
        for (; i + N <= size; i += N) {
            std::array<R, N> results;
            for (std::size_t j = 0; j < N; ++j) {
                results[j] = body(it[i + j], i + j, end_it);
            }

            for (std::size_t j = 0; j < N; ++j) {
                if (results[j] != end_it) return results[j];
            }
        }
        for (; i < size; ++i) {
            R result = body(it[i], i, end_it);
            if (result != end_it) return result;
        }
        return static_cast<R>(end_it);
    }
}

// =============================================================================
// Reduce loops (multi-accumulator for true ILP)
// =============================================================================

// Unified reduce implementation - handles both simple (no ctrl) and ctrl-enabled lambdas
template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
auto reduce_impl(T start, T end, Init init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    constexpr bool has_ctrl = std::invocable<F, T, LoopCtrl<void>&>;

    if constexpr (has_ctrl) {
        using R = std::invoke_result_t<F, T, LoopCtrl<void>&>;
        R identity = operation_identity(op, static_cast<R>(init));

        std::array<R, N> accs;
        accs.fill(identity);

        LoopCtrl<void> ctrl;
        T i = start;

        for (; i + static_cast<T>(N) <= end && ctrl.ok; i += static_cast<T>(N)) {
            for (std::size_t j = 0; j < N && ctrl.ok; ++j) {
                accs[j] = op(accs[j], body(i + static_cast<T>(j), ctrl));
            }
        }

        for (; i < end && ctrl.ok; ++i) {
            accs[0] = op(accs[0], body(i, ctrl));
        }

        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            R result = init;
            ((result = op(result, accs[Is])), ...);
            return result;
        }(std::make_index_sequence<N>{});
    } else {
        static_assert(std::invocable<F, T>, "Lambda must be invocable with (T) or (T, LoopCtrl<void>&)");
        using R = std::invoke_result_t<F, T>;
        R identity = operation_identity(op, static_cast<R>(init));

        std::array<R, N> accs;
        accs.fill(identity);

        T i = start;

        for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
            for (std::size_t j = 0; j < N; ++j) {
                accs[j] = op(accs[j], body(i + static_cast<T>(j)));
            }
        }

        for (; i < end; ++i) {
            accs[0] = op(accs[0], body(i));
        }

        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            R result = init;
            ((result = op(result, accs[Is])), ...);
            return result;
        }(std::make_index_sequence<N>{});
    }
}

// Unified range-based reduce - optimized for contiguous ranges
template<std::size_t N, std::ranges::contiguous_range Range, typename Init, typename BinaryOp, typename F>
auto reduce_range_impl(Range&& range, Init init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    using Ref = std::ranges::range_reference_t<Range>;
    constexpr bool has_ctrl = std::invocable<F, Ref, LoopCtrl<void>&>;

    if constexpr (!has_ctrl) {
        check_range_lambda_param<F, Ref>();
    }

    auto* ptr = std::ranges::data(range);
    auto size = std::ranges::size(range);
    std::size_t i = 0;

    if constexpr (has_ctrl) {
        using R = std::invoke_result_t<F, Ref, LoopCtrl<void>&>;
        R identity = operation_identity(op, static_cast<R>(init));
        std::array<R, N> accs;
        accs.fill(identity);

        LoopCtrl<void> ctrl;

        for (; i + N <= size && ctrl.ok; i += N) {
            for (std::size_t j = 0; j < N && ctrl.ok; ++j) {
                accs[j] = op(accs[j], body(ptr[i + j], ctrl));
            }
        }

        for (; i < size && ctrl.ok; ++i) {
            accs[0] = op(accs[0], body(ptr[i], ctrl));
        }

        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            R result = init;
            ((result = op(result, accs[Is])), ...);
            return result;
        }(std::make_index_sequence<N>{});
    } else {
        static_assert(std::invocable<F, Ref>, "Lambda must be invocable with (Ref) or (Ref, LoopCtrl<void>&)");
        using R = std::invoke_result_t<F, Ref>;
        R identity = operation_identity(op, static_cast<R>(init));
        std::array<R, N> accs;
        accs.fill(identity);

        for (; i + N <= size; i += N) {
            for (std::size_t j = 0; j < N; ++j) {
                accs[j] = op(accs[j], body(ptr[i + j]));
            }
        }

        for (; i < size; ++i) {
            accs[0] = op(accs[0], body(ptr[i]));
        }

        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            R result = init;
            ((result = op(result, accs[Is])), ...);
            return result;
        }(std::make_index_sequence<N>{});
    }
}

// Unified range-based reduce - fallback for random access (non-contiguous) ranges
template<std::size_t N, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires (!std::ranges::contiguous_range<Range>)
auto reduce_range_impl(Range&& range, Init init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    using Ref = std::ranges::range_reference_t<Range>;
    constexpr bool has_ctrl = std::invocable<F, Ref, LoopCtrl<void>&>;

    if constexpr (!has_ctrl) {
        check_range_lambda_param<F, Ref>();
    }

    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);
    std::size_t i = 0;

    if constexpr (has_ctrl) {
        using R = std::invoke_result_t<F, Ref, LoopCtrl<void>&>;
        R identity = operation_identity(op, static_cast<R>(init));
        std::array<R, N> accs;
        accs.fill(identity);

        LoopCtrl<void> ctrl;

        for (; i + N <= size && ctrl.ok; i += N) {
            for (std::size_t j = 0; j < N && ctrl.ok; ++j) {
                accs[j] = op(accs[j], body(it[i + j], ctrl));
            }
        }

        for (; i < size && ctrl.ok; ++i) {
            accs[0] = op(accs[0], body(it[i], ctrl));
        }

        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            R result = init;
            ((result = op(result, accs[Is])), ...);
            return result;
        }(std::make_index_sequence<N>{});
    } else {
        static_assert(std::invocable<F, Ref>, "Lambda must be invocable with (Ref) or (Ref, LoopCtrl<void>&)");
        using R = std::invoke_result_t<F, Ref>;
        R identity = operation_identity(op, static_cast<R>(init));
        std::array<R, N> accs;
        accs.fill(identity);

        for (; i + N <= size; i += N) {
            for (std::size_t j = 0; j < N; ++j) {
                accs[j] = op(accs[j], body(it[i + j]));
            }
        }

        for (; i < size; ++i) {
            accs[0] = op(accs[0], body(it[i]));
        }

        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            R result = init;
            ((result = op(result, accs[Is])), ...);
            return result;
        }(std::make_index_sequence<N>{});
    }
}

// Step-based reduce
template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_step_simple_impl(T start, T end, T step, Init init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    using R = std::invoke_result_t<F, T>;

    // Get identity element for this operation
    R identity = operation_identity(op, static_cast<R>(init));

    std::array<R, N> accs;
    accs.fill(identity);

    T i = start;
    T last_offset = step * static_cast<T>(N - 1);

    auto in_range = [&](T val) {
        return step > 0 ? val < end : val > end;
    };

    // Main unrolled loop - nested loop pattern enables universal vectorization (GCC + Clang)
    while (in_range(i) && in_range(i + last_offset)) {
        for (std::size_t j = 0; j < N; ++j) {
            accs[j] = op(accs[j], body(i + step * static_cast<T>(j)));
        }
        i += step * static_cast<T>(N);
    }

    // Remainder
    while (in_range(i)) {
        accs[0] = op(accs[0], body(i));
        i += step;
    }

    // Final reduction - apply init exactly once (unrolled via fold expression)
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        R result = init;
        ((result = op(result, accs[Is])), ...);
        return result;
    }(std::make_index_sequence<N>{});
}

// =============================================================================
// For-until loops - optimized early exit with predicate baked into loop condition
// =============================================================================

// Index-based for_until - returns index of first match
template<std::size_t N, std::integral T, typename Pred>
    requires std::invocable<Pred, T> && std::same_as<std::invoke_result_t<Pred, T>, bool>
std::optional<T> for_until_impl(T start, T end, Pred&& pred) {
    validate_unroll_factor<N>();

    // Use pragma unroll like std::find - GCC optimizes this pattern best
    _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
    for (T i = start; i < end; ++i) {
        if (pred(i)) return i;
    }

    return std::nullopt;
}

// Range-based for_until - returns index of first match
template<std::size_t N, std::ranges::random_access_range Range, typename Pred>
std::optional<std::size_t> for_until_range_impl(Range&& range, Pred&& pred) {
    validate_unroll_factor<N>();

    // Check if lambda parameter is by-value (performance warning)
    check_range_lambda_param<Pred, std::ranges::range_reference_t<Range>>();

    auto* data = std::ranges::data(range);
    std::size_t n = std::ranges::size(range);

    // Use pragma unroll like std::find - GCC optimizes this pattern best
    _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4))
    for (std::size_t i = 0; i < n; ++i) {
        if (pred(data[i])) return i;
    }

    return std::nullopt;
}

} // namespace detail

// =============================================================================
// Public API - Index-based loops
// =============================================================================

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_simple(T start, T end, F&& body) {
    detail::for_loop_impl<N>(start, end, std::forward<F>(body));
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
    requires std::invocable<F, T, T>
auto find(T start, T end, F&& body) {
    return detail::find_impl<N>(start, end, std::forward<F>(body));
}

// =============================================================================
// Public API - Step loops
// =============================================================================

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_step_simple(T start, T end, T step, F&& body) {
    detail::for_loop_step_impl<N>(start, end, step, std::forward<F>(body));
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
    requires std::invocable<F, T, T>
auto for_loop_step_ret_simple(T start, T end, T step, F&& body) {
    return detail::for_loop_step_ret_simple_impl<N>(start, end, step, std::forward<F>(body));
}

// =============================================================================
// Public API - Range-based loops
// =============================================================================

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
void for_loop_range_simple(Range&& range, F&& body) {
    detail::for_loop_range_impl<N>(std::forward<Range>(range), std::forward<F>(body));
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
auto find_range_idx(Range&& range, F&& body) {
    return detail::find_range_idx_impl<N>(std::forward<Range>(range), std::forward<F>(body));
}

// =============================================================================
// Public API - For-until loops (optimized early exit)
// =============================================================================

template<std::size_t N = 8, std::integral T, typename Pred>
    requires std::invocable<Pred, T> && std::same_as<std::invoke_result_t<Pred, T>, bool>
std::optional<T> for_until(T start, T end, Pred&& pred) {
    return detail::for_until_impl<N>(start, end, std::forward<Pred>(pred));
}

template<std::size_t N = 8, std::ranges::random_access_range Range, typename Pred>
std::optional<std::size_t> for_until_range(Range&& range, Pred&& pred) {
    return detail::for_until_range_impl<N>(std::forward<Range>(range), std::forward<Pred>(pred));
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
    requires std::invocable<F, std::ranges::range_reference_t<Range>, LoopCtrl<void>&>
auto reduce_range(Range&& range, Init init, BinaryOp op, F&& body) {
    return detail::reduce_range_impl<N>(std::forward<Range>(range), init, op, std::forward<F>(body));
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

// =============================================================================
// Auto-selecting functions
// =============================================================================

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

template<std::integral T, typename Pred>
    requires std::invocable<Pred, T> && std::same_as<std::invoke_result_t<Pred, T>, bool>
std::optional<T> for_until_auto(T start, T end, Pred&& pred) {
    return detail::for_until_impl<optimal_N<LoopType::Search, T>>(start, end, std::forward<Pred>(pred));
}

template<std::ranges::random_access_range Range, typename Pred>
std::optional<std::size_t> for_until_range_auto(Range&& range, Pred&& pred) {
    using T = std::ranges::range_value_t<Range>;
    return detail::for_until_range_impl<optimal_N<LoopType::Search, T>>(std::forward<Range>(range), std::forward<Pred>(pred));
}

template<std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, std::size_t, decltype(std::ranges::end(std::declval<Range>()))>
auto find_range_idx_auto(Range&& range, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    return find_range_idx<optimal_N<LoopType::Search, T>>(std::forward<Range>(range), std::forward<F>(body));
}

} // namespace ilp
