// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

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

        template<typename Op, typename T>
        constexpr bool has_known_identity_v =
            std::is_same_v<std::decay_t<Op>, std::plus<>> || std::is_same_v<std::decay_t<Op>, std::plus<T>> ||
            std::is_same_v<std::decay_t<Op>, std::multiplies<>> ||
            std::is_same_v<std::decay_t<Op>, std::multiplies<T>> || std::is_same_v<std::decay_t<Op>, std::bit_and<>> ||
            std::is_same_v<std::decay_t<Op>, std::bit_and<T>> || std::is_same_v<std::decay_t<Op>, std::bit_or<>> ||
            std::is_same_v<std::decay_t<Op>, std::bit_or<T>> || std::is_same_v<std::decay_t<Op>, std::bit_xor<>> ||
            std::is_same_v<std::decay_t<Op>, std::bit_xor<T>>;

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

        template<typename Op, typename T>
        constexpr T operation_identity([[maybe_unused]] const Op& op, [[maybe_unused]] T init) {
            if constexpr (has_known_identity_v<Op, T>) {
                return make_identity<Op, T>();
            } else {
                return init;
            }
        }

        template<std::size_t N, typename BinaryOp, typename R, typename Init>
        std::array<R, N> make_accumulators([[maybe_unused]] const BinaryOp& op, [[maybe_unused]] Init&& init) {
            if constexpr (has_known_identity_v<BinaryOp, R>) {
                return []<std::size_t... Is>(std::index_sequence<Is...>) {
                    return std::array<R, N>{((void)Is, make_identity<BinaryOp, R>())...};
                }(std::make_index_sequence<N>{});
            } else {
                std::array<R, N> accs;
                accs[0] = static_cast<R>(std::forward<Init>(init));
                for (std::size_t i = 1; i < N; ++i) {
                    accs[i] = accs[0];
                }
                return accs;
            }
        }

        template<std::size_t N, std::integral T, typename F>
            requires ForUntypedCtrlBody<F, T>
        ForResult for_loop_untyped_impl(T start, T end, F&& body) {
            validate_unroll_factor<N>();
            ForCtrl ctrl;
            T i = start;

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

        template<typename R, std::size_t N, std::integral T, typename F>
            requires ForTypedCtrlBody<F, T, R>
        ForResultTyped<R> for_loop_typed_impl(T start, T end, F&& body) {
            validate_unroll_factor<N>();
            ForCtrlTyped<R> ctrl;
            T i = start;

            for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
                for (std::size_t j = 0; j < N; ++j) {
                    body(i + static_cast<T>(j), ctrl);
                    if (!ctrl.ok) [[unlikely]]
                        return ForResultTyped<R>{ctrl.return_set, std::move(ctrl.storage)};
                }
            }

            for (; i < end; ++i) {
                body(i, ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return ForResultTyped<R>{ctrl.return_set, std::move(ctrl.storage)};
            }

            return ForResultTyped<R>{false, {}};
        }

        template<std::size_t N, std::integral T, typename F>
            requires FindBody<F, T>
        auto find_impl(T start, T end, F&& body) {
            validate_unroll_factor<N>();
            using R = std::invoke_result_t<F, T, T>;

            if constexpr (std::is_same_v<R, bool>) {
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

        template<std::size_t N, std::ranges::random_access_range Range, typename F>
            requires ForRangeUntypedCtrlBody<F, std::ranges::range_reference_t<Range>>
        ForResult for_loop_range_untyped_impl(Range&& range, F&& body) {
            validate_unroll_factor<N>();
            ForCtrl ctrl;
            auto it = std::ranges::begin(range);
            auto size = std::ranges::size(range);
            std::size_t i = 0;

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

        template<typename R, std::size_t N, std::ranges::random_access_range Range, typename F>
            requires ForRangeTypedCtrlBody<F, std::ranges::range_reference_t<Range>, R>
        ForResultTyped<R> for_loop_range_typed_impl(Range&& range, F&& body) {
            validate_unroll_factor<N>();
            ForCtrlTyped<R> ctrl;
            auto it = std::ranges::begin(range);
            auto size = std::ranges::size(range);
            std::size_t i = 0;

            for (; i + N <= size; i += N) {
                for (std::size_t j = 0; j < N; ++j) {
                    body(it[i + j], ctrl);
                    if (!ctrl.ok) [[unlikely]]
                        return ForResultTyped<R>{ctrl.return_set, std::move(ctrl.storage)};
                }
            }

            for (; i < size; ++i) {
                body(it[i], ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return ForResultTyped<R>{ctrl.return_set, std::move(ctrl.storage)};
            }

            return ForResultTyped<R>{false, {}};
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

            for (; i < size; ++i) {
                if (pred(it[i]))
                    return it + static_cast<std::ptrdiff_t>(i);
            }

            return end_it;
        }

        // Value-based find - returns iterator (like std::find)
        template<std::size_t N, std::ranges::random_access_range Range, typename T>
            requires std::equality_comparable_with<std::ranges::range_reference_t<Range>, const T&>
        auto find_value_impl(Range&& range, const T& value) {
            validate_unroll_factor<N>();

            auto it = std::ranges::begin(range);
            auto end_it = std::ranges::end(range);
            auto size = std::ranges::size(range);
            std::size_t i = 0;

            for (; i + N <= size; i += N) {
                std::array<bool, N> matches;
                for (std::size_t j = 0; j < N; ++j) {
                    matches[j] = (it[i + j] == value);
                }
                for (std::size_t j = 0; j < N; ++j) {
                    if (matches[j])
                        return it + static_cast<std::ptrdiff_t>(i + j);
                }
            }

            for (; i < size; ++i) {
                if (it[i] == value)
                    return it + static_cast<std::ptrdiff_t>(i);
            }

            return end_it;
        }

        template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
            requires ReduceBody<F, T>
        auto reduce_impl(T start, T end, Init&& init, BinaryOp op, F&& body) {
            validate_unroll_factor<N>();

            using ResultT = std::invoke_result_t<F, T>;

            if constexpr (is_optional_v<ResultT>) {
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

        template<std::size_t N, std::ranges::contiguous_range Range, typename Init, typename BinaryOp, typename F>
        auto reduce_range_impl(Range&& range, Init&& init, BinaryOp op, F&& body) {
            validate_unroll_factor<N>();

            auto* ptr = std::ranges::data(range);
            auto size = std::ranges::size(range);

            using ResultT = std::invoke_result_t<F, decltype(*ptr)>;

            if constexpr (is_optional_v<ResultT>) {
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
                return std::transform_reduce(ptr, ptr + size, std::forward<Init>(init), op, body);
            }
        }

        // non-contiguous ranges
        template<std::size_t N, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
            requires(!std::ranges::contiguous_range<Range>)
        auto reduce_range_impl(Range&& range, Init&& init, BinaryOp op, F&& body) {
            validate_unroll_factor<N>();

            auto it = std::ranges::begin(range);
            auto size = std::ranges::size(range);

            using ResultT = std::invoke_result_t<F, decltype(*it)>;

            if constexpr (is_optional_v<ResultT>) {
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
                return std::transform_reduce(std::ranges::begin(range), std::ranges::end(range),
                                             std::forward<Init>(init), op, body);
            }
        }

        // Direct reduce (no transform) - like std::reduce
        // Contiguous range version
        template<std::size_t N, std::ranges::contiguous_range Range, typename T, typename BinaryOp>
            requires DirectReducible<BinaryOp, T, std::ranges::range_reference_t<Range>>
        T reduce_direct_impl(Range&& range, T init, BinaryOp op) {
            validate_unroll_factor<N>();

            auto* ptr = std::ranges::data(range);
            auto size = std::ranges::size(range);

            // Use N accumulators for ILP
            auto accs = make_accumulators<N, BinaryOp, T>(op, init);

            std::size_t i = 0;
            for (; i + N <= size; i += N) {
                for (std::size_t j = 0; j < N; ++j) {
                    accs[j] = op(accs[j], ptr[i + j]);
                }
            }

            // Cleanup remaining elements
            for (; i < size; ++i) {
                accs[0] = op(accs[0], ptr[i]);
            }

            // Combine all accumulators
            return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                T out = init;
                ((out = op(out, accs[Is])), ...);
                return out;
            }(std::make_index_sequence<N>{});
        }

        // Direct reduce - non-contiguous range version
        template<std::size_t N, std::ranges::random_access_range Range, typename T, typename BinaryOp>
            requires(!std::ranges::contiguous_range<Range>) &&
                    DirectReducible<BinaryOp, T, std::ranges::range_reference_t<Range>>
        T reduce_direct_impl(Range&& range, T init, BinaryOp op) {
            validate_unroll_factor<N>();

            auto it = std::ranges::begin(range);
            auto size = std::ranges::size(range);

            // Use N accumulators for ILP
            auto accs = make_accumulators<N, BinaryOp, T>(op, init);

            std::size_t i = 0;
            for (; i + N <= size; i += N) {
                for (std::size_t j = 0; j < N; ++j) {
                    accs[j] = op(accs[j], it[i + j]);
                }
            }

            // Cleanup remaining elements
            for (; i < size; ++i) {
                accs[0] = op(accs[0], it[i]);
            }

            // Combine all accumulators
            return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                T out = init;
                ((out = op(out, accs[Is])), ...);
                return out;
            }(std::make_index_sequence<N>{});
        }

    } // namespace detail

    template<std::size_t N = 4, std::integral T, typename F>
        requires detail::ForUntypedCtrlBody<F, T>
    ForResult for_loop(T start, T end, F&& body) {
        return detail::for_loop_untyped_impl<N>(start, end, std::forward<F>(body));
    }

    template<typename R, std::size_t N = 4, std::integral T, typename F>
        requires detail::ForTypedCtrlBody<F, T, R>
    ForResultTyped<R> for_loop_typed(T start, T end, F&& body) {
        return detail::for_loop_typed_impl<R, N>(start, end, std::forward<F>(body));
    }

    // Value-based find - returns iterator (like std::find)
    template<std::size_t N = 4, std::ranges::random_access_range Range, typename T>
        requires std::equality_comparable_with<std::ranges::range_reference_t<Range>, const T&>
    auto find(Range&& range, const T& value) {
        return detail::find_value_impl<N>(std::forward<Range>(range), value);
    }

    // Auto-selecting N for value-based find
    template<LoopType LT = LoopType::Search, std::ranges::random_access_range Range, typename T>
        requires std::equality_comparable_with<std::ranges::range_reference_t<Range>, const T&>
    auto find_auto(Range&& range, const T& value) {
        using ElemT = std::ranges::range_value_t<Range>;
        return detail::find_value_impl<optimal_N<LT, ElemT>>(std::forward<Range>(range), value);
    }

    template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
        requires detail::ForRangeUntypedCtrlBody<F, std::ranges::range_reference_t<Range>>
    ForResult for_loop_range(Range&& range, F&& body) {
        return detail::for_loop_range_untyped_impl<N>(std::forward<Range>(range), std::forward<F>(body));
    }

    template<typename R, std::size_t N = 4, std::ranges::random_access_range Range, typename F>
        requires detail::ForRangeTypedCtrlBody<F, std::ranges::range_reference_t<Range>, R>
    ForResultTyped<R> for_loop_range_typed(Range&& range, F&& body) {
        return detail::for_loop_range_typed_impl<R, N>(std::forward<Range>(range), std::forward<F>(body));
    }

    template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
    auto for_loop_range_ret_simple(Range&& range, F&& body) {
        return detail::for_loop_range_ret_simple_impl<N>(std::forward<Range>(range), std::forward<F>(body));
    }

    template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
    auto find_range_idx(Range&& range, F&& body) {
        return detail::find_range_idx_impl<N>(std::forward<Range>(range), std::forward<F>(body));
    }

    // Predicate-based find_if - returns iterator (like std::find_if)
    template<std::size_t N = 4, std::ranges::random_access_range Range, typename Pred>
        requires std::predicate<Pred, std::ranges::range_reference_t<Range>>
    auto find_if(Range&& range, Pred&& pred) {
        return detail::find_range_impl<N>(std::forward<Range>(range), std::forward<Pred>(pred));
    }

    // Auto-selecting N for predicate-based find_if
    template<LoopType LT = LoopType::Search, std::ranges::random_access_range Range, typename Pred>
        requires std::predicate<Pred, std::ranges::range_reference_t<Range>>
    auto find_if_auto(Range&& range, Pred&& pred) {
        using T = std::ranges::range_value_t<Range>;
        return detail::find_range_impl<optimal_N<LT, T>>(std::forward<Range>(range), std::forward<Pred>(pred));
    }

    // Direct reduce (no transform) - like std::reduce
    template<std::size_t N = 4, std::ranges::random_access_range Range, typename T, typename BinaryOp = std::plus<>>
        requires detail::DirectReducible<BinaryOp, T, std::ranges::range_reference_t<Range>>
    T reduce(Range&& range, T init, BinaryOp op = {}) {
        return detail::reduce_direct_impl<N>(std::forward<Range>(range), std::move(init), op);
    }

    // Auto-selecting N for direct reduce
    template<LoopType LT, std::ranges::random_access_range Range, typename T, typename BinaryOp = std::plus<>>
        requires detail::DirectReducible<BinaryOp, T, std::ranges::range_reference_t<Range>>
    T reduce_auto(Range&& range, T init, BinaryOp op = {}) {
        using ElemT = std::ranges::range_value_t<Range>;
        return detail::reduce_direct_impl<optimal_N<LT, ElemT>>(std::forward<Range>(range), std::move(init), op);
    }

    // Transform then reduce - like std::transform_reduce
    template<std::size_t N = 4, std::ranges::random_access_range Range, typename T, typename BinaryOp, typename UnaryOp>
        requires detail::TransformReducible<UnaryOp, BinaryOp, T, std::ranges::range_reference_t<Range>>
    T transform_reduce(Range&& range, T init, BinaryOp reduce_op, UnaryOp transform) {
        return detail::reduce_range_impl<N>(std::forward<Range>(range), std::move(init), reduce_op,
                                            std::forward<UnaryOp>(transform));
    }

    // Auto-selecting N for transform_reduce
    template<LoopType LT, std::ranges::random_access_range Range, typename T, typename BinaryOp, typename UnaryOp>
        requires detail::TransformReducible<UnaryOp, BinaryOp, T, std::ranges::range_reference_t<Range>>
    T transform_reduce_auto(Range&& range, T init, BinaryOp reduce_op, UnaryOp transform) {
        using ElemT = std::ranges::range_value_t<Range>;
        return detail::reduce_range_impl<optimal_N<LT, ElemT>>(std::forward<Range>(range), std::move(init), reduce_op,
                                                               std::forward<UnaryOp>(transform));
    }

    template<LoopType LT, std::integral T, typename F>
        requires detail::ForUntypedCtrlBody<F, T>
    ForResult for_loop_auto(T start, T end, F&& body) {
        return for_loop<optimal_N<LT, T>>(start, end, std::forward<F>(body));
    }

    template<typename R, LoopType LT, std::integral T, typename F>
        requires detail::ForTypedCtrlBody<F, T, R>
    ForResultTyped<R> for_loop_typed_auto(T start, T end, F&& body) {
        return for_loop_typed<R, optimal_N<LT, T>>(start, end, std::forward<F>(body));
    }

    template<LoopType LT, std::ranges::random_access_range Range, typename F>
        requires detail::ForRangeUntypedCtrlBody<F, std::ranges::range_reference_t<Range>>
    ForResult for_loop_range_auto(Range&& range, F&& body) {
        using T = std::ranges::range_value_t<Range>;
        return for_loop_range<optimal_N<LT, T>>(std::forward<Range>(range), std::forward<F>(body));
    }

    template<typename R, LoopType LT, std::ranges::random_access_range Range, typename F>
        requires detail::ForRangeTypedCtrlBody<F, std::ranges::range_reference_t<Range>, R>
    ForResultTyped<R> for_loop_range_typed_auto(Range&& range, F&& body) {
        using T = std::ranges::range_value_t<Range>;
        return for_loop_range_typed<R, optimal_N<LT, T>>(std::forward<Range>(range), std::forward<F>(body));
    }


    template<LoopType LT = LoopType::Search, std::ranges::random_access_range Range, typename F>
        requires std::invocable<F, std::ranges::range_reference_t<Range>, std::size_t,
                                decltype(std::ranges::end(std::declval<Range>()))>
    auto find_range_idx_auto(Range&& range, F&& body) {
        using T = std::ranges::range_value_t<Range>;
        return find_range_idx<optimal_N<LT, T>>(std::forward<Range>(range), std::forward<F>(body));
    }

} // namespace ilp
