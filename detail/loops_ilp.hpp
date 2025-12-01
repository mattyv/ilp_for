#pragma once

// Full ILP loop implementations - multi-accumulator pattern for latency hiding
// Best for Clang which generates ccmp chains

#include <array>
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
// Identity element inference for common operations
// =============================================================================

// Trait to detect operations with compile-time known identity elements
template<typename Op, typename T>
constexpr bool has_known_identity_v =
    std::is_same_v<std::decay_t<Op>, std::plus<>> ||
    std::is_same_v<std::decay_t<Op>, std::plus<T>> ||
    std::is_same_v<std::decay_t<Op>, std::multiplies<>> ||
    std::is_same_v<std::decay_t<Op>, std::multiplies<T>> ||
    std::is_same_v<std::decay_t<Op>, std::bit_and<>> ||
    std::is_same_v<std::decay_t<Op>, std::bit_and<T>> ||
    std::is_same_v<std::decay_t<Op>, std::bit_or<>> ||
    std::is_same_v<std::decay_t<Op>, std::bit_or<T>> ||
    std::is_same_v<std::decay_t<Op>, std::bit_xor<>> ||
    std::is_same_v<std::decay_t<Op>, std::bit_xor<T>>;

// Construct identity element directly (zero copies)
template<typename Op, typename T>
constexpr T make_identity() {
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
        static_assert(has_known_identity_v<Op, T>, "make_identity requires known operation");
    }
}

// Legacy: infer identity from init (copies init for unknown ops)
template<typename Op, typename T>
constexpr T operation_identity([[maybe_unused]] const Op& op, [[maybe_unused]] T init) {
    if constexpr (has_known_identity_v<Op, T>) {
        return make_identity<Op, T>();
    } else {
        // For unknown operations (lambdas, etc.), fall back to using init as identity
        // This is only correct if init is actually the identity element
        return init;
    }
}

// Create accumulator array - zero copies for known ops, N copies for unknown ops
template<std::size_t N, typename BinaryOp, typename R, typename Init>
std::array<R, N> make_accumulators([[maybe_unused]] const BinaryOp& op, [[maybe_unused]] Init&& init) {
    if constexpr (has_known_identity_v<BinaryOp, R>) {
        // Zero-copy: directly construct identity elements via guaranteed copy elision
        return []<std::size_t... Is>(std::index_sequence<Is...>) {
            return std::array<R, N>{((void)Is, make_identity<BinaryOp, R>())...};
        }(std::make_index_sequence<N>{});
    } else {
        // Unknown ops: copy identity to all elements (N copies)
        std::array<R, N> accs;
        R identity = operation_identity(op, static_cast<R>(std::forward<Init>(init)));
        accs.fill(identity);
        return accs;
    }
}

// =============================================================================
// Index-based loops
// =============================================================================

// Unified for_loop implementation - handles both simple (no ctrl) and ctrl-enabled lambdas
template<std::size_t N, std::integral T, typename F>
    requires ForBody<F, T> || ForCtrlBody<F, T>
void for_loop_impl(T start, T end, F&& body) {
    validate_unroll_factor<N>();
    constexpr bool has_ctrl = ForCtrlBody<F, T>;

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
        static_assert(ForBody<F, T>, "Lambda must be invocable with (T) or (T, LoopCtrl<void>&)");
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
    requires ForRetBody<F, T, R>
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
    requires FindBody<F, T>
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
                if (results[j].has_value()) return std::move(results[j]);
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
                if (results[j] != end) return std::move(results[j]);
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
// Range-based loops
// =============================================================================

// Unified for_loop_range implementation - handles both simple (no ctrl) and ctrl-enabled lambdas
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
                if (results[j].has_value()) return std::move(results[j]);
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
                if (results[j] != end_it) return std::move(results[j]);
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
                if (results[j].has_value()) return std::move(results[j]);
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
                if (results[j] != end_it) return std::move(results[j]);
            }
        }
        for (; i < size; ++i) {
            R result = body(it[i], i, end_it);
            if (result != end_it) return result;
        }
        return static_cast<R>(end_it);
    }
}

// Range-based find with simple bool predicate - returns iterator
template<std::size_t N, std::ranges::random_access_range Range, typename Pred>
    requires PredicateRangeBody<Pred, std::ranges::range_reference_t<Range>>
auto find_range_impl(Range&& range, Pred&& pred) {
    validate_unroll_factor<N>();

    auto it = std::ranges::begin(range);
    auto end_it = std::ranges::end(range);
    auto size = std::ranges::size(range);
    std::size_t i = 0;

    // Unrolled main loop
    for (; i + N <= size; i += N) {
        std::array<bool, N> matches;
        for (std::size_t j = 0; j < N; ++j) {
            matches[j] = pred(it[i + j]);
        }
        for (std::size_t j = 0; j < N; ++j) {
            if (matches[j]) return it + static_cast<std::ptrdiff_t>(i + j);
        }
    }

    // Cleanup loop
    for (; i < size; ++i) {
        if (pred(it[i])) return it + static_cast<std::ptrdiff_t>(i);
    }

    return end_it;  // Not found sentinel
}

// =============================================================================
// Reduce loops (multi-accumulator for true ILP)
// =============================================================================

// Unified reduce implementation - handles both simple (no ctrl) and ctrl-enabled lambdas
// Supports new ReduceResult return type for auto ctrl/non-ctrl path selection
template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires ReduceBody<F, T> || ReduceCtrlBody<F, T>
auto reduce_impl(T start, T end, Init&& init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    constexpr bool has_ctrl = ReduceCtrlBody<F, T>;

    if constexpr (has_ctrl) {
        using ResultT = std::invoke_result_t<F, T, LoopCtrl<void>&>;

        if constexpr (is_reduce_result_v<ResultT>) {
            // New API: ILP_REDUCE_RETURN/ILP_REDUCE_BREAK
            using R = decltype(std::declval<ResultT>().value);
            auto accs = make_accumulators<N, BinaryOp, R>(op, std::forward<Init>(init));

            LoopCtrl<void> ctrl;
            T i = start;
            bool should_break = false;

            for (; i + static_cast<T>(N) <= end && !should_break; i += static_cast<T>(N)) {
                for (std::size_t j = 0; j < N && !should_break; ++j) {
                    auto result = body(i + static_cast<T>(j), ctrl);
                    if (result.did_break()) {
                        should_break = true;
                    } else {
                        accs[j] = op(accs[j], result.value);
                    }
                }
            }

            for (; i < end && !should_break; ++i) {
                auto result = body(i, ctrl);
                if (result.did_break()) {
                    should_break = true;
                } else {
                    accs[0] = op(accs[0], result.value);
                }
            }

            return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                R result = std::forward<Init>(init);
                ((result = op(result, accs[Is])), ...);
                return result;
            }(std::make_index_sequence<N>{});
        } else {
            // Simple API: plain return value (no break support)
            using R = ResultT;
            auto accs = make_accumulators<N, BinaryOp, R>(op, std::forward<Init>(init));

            LoopCtrl<void> ctrl;
            T i = start;

            for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
                for (std::size_t j = 0; j < N; ++j) {
                    accs[j] = op(accs[j], body(i + static_cast<T>(j), ctrl));
                }
            }

            for (; i < end; ++i) {
                accs[0] = op(accs[0], body(i, ctrl));
            }

            return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                R result = std::forward<Init>(init);
                ((result = op(result, accs[Is])), ...);
                return result;
            }(std::make_index_sequence<N>{});
        }
    } else {
        static_assert(ReduceBody<F, T>, "Lambda must be invocable with (T) or (T, LoopCtrl<void>&)");
        using R = std::invoke_result_t<F, T>;
        auto accs = make_accumulators<N, BinaryOp, R>(op, std::forward<Init>(init));

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
            R result = std::forward<Init>(init);
            ((result = op(result, accs[Is])), ...);
            return result;
        }(std::make_index_sequence<N>{});
    }
}

// Unified range-based reduce - optimized for contiguous ranges
// Supports new ReduceResult return type for auto ctrl/non-ctrl path selection
template<std::size_t N, std::ranges::contiguous_range Range, typename Init, typename BinaryOp, typename F>
auto reduce_range_impl(Range&& range, Init&& init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    using Ref = std::ranges::range_reference_t<Range>;
    constexpr bool has_ctrl = ReduceRangeCtrlBody<F, Ref>;

    auto* ptr = std::ranges::data(range);
    auto size = std::ranges::size(range);
    std::size_t i = 0;

    if constexpr (has_ctrl) {
        using ResultT = std::invoke_result_t<F, Ref, LoopCtrl<void>&>;

        if constexpr (is_reduce_result_v<ResultT>) {
            // New API: ILP_REDUCE_RETURN/ILP_REDUCE_BREAK
            using R = decltype(std::declval<ResultT>().value);
            auto accs = make_accumulators<N, BinaryOp, R>(op, std::forward<Init>(init));

            LoopCtrl<void> ctrl;
            bool should_break = false;

            for (; i + N <= size && !should_break; i += N) {
                for (std::size_t j = 0; j < N && !should_break; ++j) {
                    auto result = body(ptr[i + j], ctrl);
                    if (result.did_break()) {
                        should_break = true;
                    } else {
                        accs[j] = op(accs[j], result.value);
                    }
                }
            }

            for (; i < size && !should_break; ++i) {
                auto result = body(ptr[i], ctrl);
                if (result.did_break()) {
                    should_break = true;
                } else {
                    accs[0] = op(accs[0], result.value);
                }
            }

            return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                R result = std::forward<Init>(init);
                ((result = op(result, accs[Is])), ...);
                return result;
            }(std::make_index_sequence<N>{});
        } else {
            // Simple API: plain return value (no break support)
            using R = ResultT;
            auto accs = make_accumulators<N, BinaryOp, R>(op, std::forward<Init>(init));

            LoopCtrl<void> ctrl;

            for (; i + N <= size; i += N) {
                for (std::size_t j = 0; j < N; ++j) {
                    accs[j] = op(accs[j], body(ptr[i + j], ctrl));
                }
            }

            for (; i < size; ++i) {
                accs[0] = op(accs[0], body(ptr[i], ctrl));
            }

            return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                R result = std::forward<Init>(init);
                ((result = op(result, accs[Is])), ...);
                return result;
            }(std::make_index_sequence<N>{});
        }
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

// Unified range-based reduce - fallback for random access (non-contiguous) ranges
// Supports new ReduceResult return type for auto ctrl/non-ctrl path selection
template<std::size_t N, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires (!std::ranges::contiguous_range<Range>)
auto reduce_range_impl(Range&& range, Init&& init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    using Ref = std::ranges::range_reference_t<Range>;
    constexpr bool has_ctrl = ReduceRangeCtrlBody<F, Ref>;

    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);
    std::size_t i = 0;

    if constexpr (has_ctrl) {
        using ResultT = std::invoke_result_t<F, Ref, LoopCtrl<void>&>;

        if constexpr (is_reduce_result_v<ResultT>) {
            // New API: ILP_REDUCE_RETURN/ILP_REDUCE_BREAK
            using R = decltype(std::declval<ResultT>().value);
            auto accs = make_accumulators<N, BinaryOp, R>(op, std::forward<Init>(init));

            LoopCtrl<void> ctrl;
            bool should_break = false;

            for (; i + N <= size && !should_break; i += N) {
                for (std::size_t j = 0; j < N && !should_break; ++j) {
                    auto result = body(it[i + j], ctrl);
                    if (result.did_break()) {
                        should_break = true;
                    } else {
                        accs[j] = op(accs[j], result.value);
                    }
                }
            }

            for (; i < size && !should_break; ++i) {
                auto result = body(it[i], ctrl);
                if (result.did_break()) {
                    should_break = true;
                } else {
                    accs[0] = op(accs[0], result.value);
                }
            }

            return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                R result = std::forward<Init>(init);
                ((result = op(result, accs[Is])), ...);
                return result;
            }(std::make_index_sequence<N>{});
        } else {
            // Simple API: plain return value (no break support)
            using R = ResultT;
            auto accs = make_accumulators<N, BinaryOp, R>(op, std::forward<Init>(init));

            LoopCtrl<void> ctrl;

            for (; i + N <= size; i += N) {
                for (std::size_t j = 0; j < N; ++j) {
                    accs[j] = op(accs[j], body(it[i + j], ctrl));
                }
            }

            for (; i < size; ++i) {
                accs[0] = op(accs[0], body(it[i], ctrl));
            }

            return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                R result = std::forward<Init>(init);
                ((result = op(result, accs[Is])), ...);
                return result;
            }(std::make_index_sequence<N>{});
        }
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
// For-until loops - optimized early exit with predicate baked into loop condition
// =============================================================================

// Index-based for_until - returns index of first match
template<std::size_t N, std::integral T, typename Pred>
    requires PredicateBody<Pred, T>
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
    requires detail::ForBody<F, T> || detail::ForCtrlBody<F, T>
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
// Public API - Range-based loops
// =============================================================================

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
    requires detail::ForRangeBody<F, std::ranges::range_reference_t<Range>>
          || detail::ForRangeCtrlBody<F, std::ranges::range_reference_t<Range>>
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

// =============================================================================
// Auto-selecting functions
// =============================================================================

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
