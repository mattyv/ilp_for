#pragma once

// Full ILP loop implementations - multi-accumulator pattern for latency hiding
// Best for Clang which generates ccmp chains

#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <numeric>
#include <ranges>
#include <utility>

#include "ctrl.hpp"
#include "loops_common.hpp"

namespace ilp {
    namespace detail {

        // =============================================================================
        // Identity element inference for common operations
        // =============================================================================

        // Trait to detect operations with compile-time known identity elements
        template<typename Op, typename T>
        constexpr bool has_known_identity_v =
            std::is_same_v<std::decay_t<Op>, std::plus<>> || std::is_same_v<std::decay_t<Op>, std::plus<T>> ||
            std::is_same_v<std::decay_t<Op>, std::multiplies<>> ||
            std::is_same_v<std::decay_t<Op>, std::multiplies<T>> || std::is_same_v<std::decay_t<Op>, std::bit_and<>> ||
            std::is_same_v<std::decay_t<Op>, std::bit_and<T>> || std::is_same_v<std::decay_t<Op>, std::bit_or<>> ||
            std::is_same_v<std::decay_t<Op>, std::bit_or<T>> || std::is_same_v<std::decay_t<Op>, std::bit_xor<>> ||
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
                return ~T{}; // All 1s
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
                // Unknown ops: move first, copy rest ((N-1) copies for rvalue init)
                std::array<R, N> accs;
                accs[0] = static_cast<R>(std::forward<Init>(init));
                for (std::size_t i = 1; i < N; ++i) {
                    accs[i] = accs[0];
                }
                return accs;
            }
        }

        // =============================================================================
        // Index-based loops
        // =============================================================================

        // Type-erased for_loop (new API - no return type parameter)
        template<std::size_t N, std::integral T, typename F>
            requires ForUntypedCtrlBody<F, T>
        ForResult for_loop_untyped_impl(T start, T end, F&& body) {
            validate_unroll_factor<N>();
            ForCtrl ctrl;
            T i = start;

            // Main unrolled loop - explicit early-exit check generates proper branches
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
                        if (matches[j])
                            return i + static_cast<T>(j);
                    }
                }
                for (; i < end; ++i) {
                    if (body(i, end))
                        return i;
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
                        if (results[j].has_value())
                            return std::move(results[j]);
                    }
                }
                for (; i < end; ++i) {
                    R result = body(i, end);
                    if (result.has_value())
                        return result;
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
                        if (results[j] != end)
                            return std::move(results[j]);
                    }
                }
                for (; i < end; ++i) {
                    R result = body(i, end);
                    if (result != end)
                        return result;
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

        // Type-erased range for_loop (new API - no return type parameter)
        template<std::size_t N, std::ranges::random_access_range Range, typename F>
            requires ForRangeUntypedCtrlBody<F, std::ranges::range_reference_t<Range>>
        ForResult for_loop_range_untyped_impl(Range&& range, F&& body) {
            validate_unroll_factor<N>();
            ForCtrl ctrl;
            auto it = std::ranges::begin(range);
            auto size = std::ranges::size(range);
            std::size_t i = 0;

            // Main unrolled loop - explicit early-exit check generates proper branches
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
                // Optional mode - return first with value
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
                // Value mode with sentinel
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

        // Range-based early-return with index (simple - no control flow)
        template<std::size_t N, std::ranges::random_access_range Range, typename F>
            requires std::invocable<F, std::ranges::range_reference_t<Range>, std::size_t,
                                    decltype(std::ranges::end(std::declval<Range>()))>
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
                        if (matches[j])
                            return it + (i + j);
                    }
                }
                for (; i < size; ++i) {
                    if (body(it[i], i, end_it))
                        return it + i;
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
                        if (results[j].has_value())
                            return std::move(results[j]);
                    }
                }
                for (; i < size; ++i) {
                    R result = body(it[i], i, end_it);
                    if (result.has_value())
                        return result;
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
                        if (results[j] != end_it)
                            return std::move(results[j]);
                    }
                }
                for (; i < size; ++i) {
                    R result = body(it[i], i, end_it);
                    if (result != end_it)
                        return result;
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
                    if (matches[j])
                        return it + static_cast<std::ptrdiff_t>(i + j);
                }
            }

            // Cleanup loop
            for (; i < size; ++i) {
                if (pred(it[i]))
                    return it + static_cast<std::ptrdiff_t>(i);
            }

            return end_it; // Not found sentinel
        }

        // =============================================================================
        // Reduce loops (multi-accumulator for true ILP)
        // =============================================================================

        // Reduce implementation - body returns T or std::optional<T> (nullopt = break)
        template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
            requires ReduceBody<F, T>
        auto reduce_impl(T start, T end, Init&& init, BinaryOp op, F&& body) {
            validate_unroll_factor<N>();

            using ResultT = std::invoke_result_t<F, T>;

            if constexpr (is_optional_v<ResultT>) {
                // Optional path - supports early break via nullopt
                using R = typename ResultT::value_type;
                auto accs = make_accumulators<N, BinaryOp, R>(op, std::forward<Init>(init));

                T i = start;
                bool should_break = false;

                for (; i + static_cast<T>(N) <= end && !should_break; i += static_cast<T>(N)) {
                    for (std::size_t j = 0; j < N && !should_break; ++j) {
                        auto result = body(i + static_cast<T>(j));
                        if (!result) {
                            should_break = true;
                        } else {
                            accs[j] = op(accs[j], *result);
                        }
                    }
                }

                for (; i < end && !should_break; ++i) {
                    auto result = body(i);
                    if (!result) {
                        should_break = true;
                    } else {
                        accs[0] = op(accs[0], *result);
                    }
                }

                return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    R out = std::forward<Init>(init);
                    ((out = op(out, accs[Is])), ...);
                    return out;
                }(std::make_index_sequence<N>{});
            } else {
                // Plain value path - no early break, SIMD friendly
                using R = ResultT;
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
                    R out = std::forward<Init>(init);
                    ((out = op(out, accs[Is])), ...);
                    return out;
                }(std::make_index_sequence<N>{});
            }
        }

        // Range-based reduce - body returns T or std::optional<T> (nullopt = break)
        template<std::size_t N, std::ranges::contiguous_range Range, typename Init, typename BinaryOp, typename F>
        auto reduce_range_impl(Range&& range, Init&& init, BinaryOp op, F&& body) {
            validate_unroll_factor<N>();

            auto* ptr = std::ranges::data(range);
            auto size = std::ranges::size(range);

            using ResultT = std::invoke_result_t<F, decltype(*ptr)>;

            if constexpr (is_optional_v<ResultT>) {
                // Optional path - supports early break via nullopt
                using R = typename ResultT::value_type;
                auto accs = make_accumulators<N, BinaryOp, R>(op, std::forward<Init>(init));

                std::size_t i = 0;
                bool should_break = false;

                for (; i + N <= size && !should_break; i += N) {
                    for (std::size_t j = 0; j < N && !should_break; ++j) {
                        auto result = body(ptr[i + j]);
                        if (!result) {
                            should_break = true;
                        } else {
                            accs[j] = op(accs[j], *result);
                        }
                    }
                }

                for (; i < size && !should_break; ++i) {
                    auto result = body(ptr[i]);
                    if (!result) {
                        should_break = true;
                    } else {
                        accs[0] = op(accs[0], *result);
                    }
                }

                return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    R out = std::forward<Init>(init);
                    ((out = op(out, accs[Is])), ...);
                    return out;
                }(std::make_index_sequence<N>{});
            } else {
                // Plain value → std::transform_reduce (SIMD vectorization!)
                return std::transform_reduce(ptr, ptr + size, std::forward<Init>(init), op, body);
            }
        }

        // Range-based reduce - fallback for random access (non-contiguous) ranges
        template<std::size_t N, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
            requires(!std::ranges::contiguous_range<Range>)
        auto reduce_range_impl(Range&& range, Init&& init, BinaryOp op, F&& body) {
            validate_unroll_factor<N>();

            auto it = std::ranges::begin(range);
            auto size = std::ranges::size(range);

            using ResultT = std::invoke_result_t<F, decltype(*it)>;

            if constexpr (is_optional_v<ResultT>) {
                // Optional path - supports early break via nullopt
                using R = typename ResultT::value_type;
                auto accs = make_accumulators<N, BinaryOp, R>(op, std::forward<Init>(init));

                std::size_t i = 0;
                bool should_break = false;

                for (; i + N <= size && !should_break; i += N) {
                    for (std::size_t j = 0; j < N && !should_break; ++j) {
                        auto result = body(it[i + j]);
                        if (!result) {
                            should_break = true;
                        } else {
                            accs[j] = op(accs[j], *result);
                        }
                    }
                }

                for (; i < size && !should_break; ++i) {
                    auto result = body(it[i]);
                    if (!result) {
                        should_break = true;
                    } else {
                        accs[0] = op(accs[0], *result);
                    }
                }

                return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    R out = std::forward<Init>(init);
                    ((out = op(out, accs[Is])), ...);
                    return out;
                }(std::make_index_sequence<N>{});
            } else {
                // Plain value → std::transform_reduce (SIMD vectorization!)
                return std::transform_reduce(std::ranges::begin(range), std::ranges::end(range),
                                             std::forward<Init>(init), op, body);
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
            _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4)) for (T i = start; i < end; ++i) {
                if (pred(i))
                    return i;
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
            _Pragma(ILP_PRAGMA_STR(GCC unroll ILP_N_SEARCH_4)) for (std::size_t i = 0; i < n; ++i) {
                if (pred(data[i]))
                    return i;
            }

            return std::nullopt;
        }

    } // namespace detail

    // =============================================================================
    // Public API - Index-based loops
    // =============================================================================

    // Type-erased for_loop - return type deduced from function context
    template<std::size_t N = 4, std::integral T, typename F>
        requires detail::ForUntypedCtrlBody<F, T>
    ForResult for_loop(T start, T end, F&& body) {
        return detail::for_loop_untyped_impl<N>(start, end, std::forward<F>(body));
    }

    template<std::size_t N = 4, std::integral T, typename F>
        requires std::invocable<F, T, T>
    auto find(T start, T end, F&& body) {
        return detail::find_impl<N>(start, end, std::forward<F>(body));
    }

    // =============================================================================
    // Public API - Range-based loops
    // =============================================================================

    // Type-erased for_loop_range - return type deduced from function context
    template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
        requires detail::ForRangeUntypedCtrlBody<F, std::ranges::range_reference_t<Range>>
    ForResult for_loop_range(Range&& range, F&& body) {
        return detail::for_loop_range_untyped_impl<N>(std::forward<Range>(range), std::forward<F>(body));
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
        requires std::invocable<Pred, std::ranges::range_reference_t<Range>> &&
                 std::same_as<std::invoke_result_t<Pred, std::ranges::range_reference_t<Range>>, bool>
    auto find_range(Range&& range, Pred&& pred) {
        return detail::find_range_impl<N>(std::forward<Range>(range), std::forward<Pred>(pred));
    }

    // Auto-selecting N based on CPU profile
    template<std::ranges::random_access_range Range, typename Pred>
        requires std::invocable<Pred, std::ranges::range_reference_t<Range>> &&
                 std::same_as<std::invoke_result_t<Pred, std::ranges::range_reference_t<Range>>, bool>
    auto find_range_auto(Range&& range, Pred&& pred) {
        using T = std::ranges::range_value_t<Range>;
        return detail::find_range_impl<optimal_N<LoopType::Search, T>>(std::forward<Range>(range),
                                                                       std::forward<Pred>(pred));
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
        requires detail::ReduceBody<F, T>
    auto reduce(T start, T end, Init&& init, BinaryOp op, F&& body) {
        return detail::reduce_impl<N>(start, end, std::forward<Init>(init), op, std::forward<F>(body));
    }

    template<std::size_t N = 4, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
        requires detail::ReduceRangeBody<F, std::ranges::range_reference_t<Range>>
    auto reduce_range(Range&& range, Init&& init, BinaryOp op, F&& body) {
        return detail::reduce_range_impl<N>(std::forward<Range>(range), std::forward<Init>(init), op,
                                            std::forward<F>(body));
    }

    // =============================================================================
    // Auto-selecting functions
    // =============================================================================

    // Type-erased auto-selecting for_loop
    template<std::integral T, typename F>
        requires detail::ForUntypedCtrlBody<F, T>
    ForResult for_loop_auto(T start, T end, F&& body) {
        return for_loop<optimal_N<LoopType::Sum, T>>(start, end, std::forward<F>(body));
    }

    template<std::ranges::random_access_range Range, typename F>
        requires detail::ForRangeUntypedCtrlBody<F, std::ranges::range_reference_t<Range>>
    ForResult for_loop_range_auto(Range&& range, F&& body) {
        using T = std::ranges::range_value_t<Range>;
        return for_loop_range<optimal_N<LoopType::Sum, T>>(std::forward<Range>(range), std::forward<F>(body));
    }

    template<std::integral T, typename F>
        requires std::invocable<F, T, T>
    auto find_auto(T start, T end, F&& body) {
        return detail::find_impl<optimal_N<LoopType::Search, T>>(start, end, std::forward<F>(body));
    }

    template<std::integral T, typename Init, typename BinaryOp, typename F>
        requires detail::ReduceBody<F, T>
    auto reduce_auto(T start, T end, Init&& init, BinaryOp op, F&& body) {
        return reduce<optimal_N<LoopType::Sum, T>>(start, end, std::forward<Init>(init), op, std::forward<F>(body));
    }

    template<std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
        requires detail::ReduceRangeBody<F, std::ranges::range_reference_t<Range>>
    auto reduce_range_auto(Range&& range, Init&& init, BinaryOp op, F&& body) {
        using T = std::ranges::range_value_t<Range>;
        return reduce_range<optimal_N<LoopType::Sum, T>>(std::forward<Range>(range), std::forward<Init>(init), op,
                                                         std::forward<F>(body));
    }

    template<std::integral T, typename Pred>
        requires std::invocable<Pred, T> && std::same_as<std::invoke_result_t<Pred, T>, bool>
    std::optional<T> for_until_auto(T start, T end, Pred&& pred) {
        return detail::for_until_impl<optimal_N<LoopType::Search, T>>(start, end, std::forward<Pred>(pred));
    }

    template<std::ranges::random_access_range Range, typename Pred>
    std::optional<std::size_t> for_until_range_auto(Range&& range, Pred&& pred) {
        using T = std::ranges::range_value_t<Range>;
        return detail::for_until_range_impl<optimal_N<LoopType::Search, T>>(std::forward<Range>(range),
                                                                            std::forward<Pred>(pred));
    }

    template<std::ranges::random_access_range Range, typename F>
        requires std::invocable<F, std::ranges::range_reference_t<Range>, std::size_t,
                                decltype(std::ranges::end(std::declval<Range>()))>
    auto find_range_idx_auto(Range&& range, F&& body) {
        using T = std::ranges::range_value_t<Range>;
        return find_range_idx<optimal_N<LoopType::Search, T>>(std::forward<Range>(range), std::forward<F>(body));
    }

} // namespace ilp
