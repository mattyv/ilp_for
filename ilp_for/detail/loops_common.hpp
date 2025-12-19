// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <concepts>
#include <cstddef>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>

#include "ctrl.hpp"

namespace ilp {
    namespace detail {

        // void sentinel - makes "return *ret" compile in void fns
        struct no_return_t {
            constexpr explicit operator bool() const noexcept { return false; }
            [[noreturn]] void operator*() const noexcept {
#if defined(__cpp_lib_unreachable) && __cpp_lib_unreachable >= 202202L
                std::unreachable();
#elif defined(__GNUC__) || defined(__clang__)
                __builtin_unreachable();
#elif defined(_MSC_VER)
                __assume(false);
#endif
            }
        };

        template<typename R>
        using for_result_t = std::conditional_t<std::is_void_v<R>, no_return_t, std::optional<R>>;

        template<std::size_t N>
        [[deprecated("Unroll factor N > 16 is likely counterproductive: "
                     "exceeds CPU execution port throughput and causes instruction cache bloat. "
                     "Typical optimal values are 4-8.")]]
        constexpr void warn_large_unroll_factor() {}

        template<std::size_t N>
        constexpr void validate_unroll_factor() {
            static_assert(N >= 1, "Unroll factor N must be at least 1");
            if constexpr (N > 16) {
                warn_large_unroll_factor<N>();
            }
        }

        template<typename T>
        concept signed_integral_type = std::integral<T> && std::signed_integral<T>;

        template<typename T>
        concept unsigned_integral_type = std::integral<T> && std::unsigned_integral<T>;

        template<typename AccumT, typename ElemT>
        [[deprecated("Overflow risk: accumulator type may be too small for sum. "
                     "Consider using a larger type (e.g., int64_t or double) or "
                     "explicitly provide an init value with sufficient range. "
                     "For small, bounded ranges this warning can be safely ignored.")]]
        constexpr void warn_accumulator_overflow() {}

        template<typename AccumT, typename ElemT>
        constexpr void check_sum_overflow() {
            if constexpr (std::integral<AccumT> && std::integral<ElemT>) {
                if constexpr (sizeof(AccumT) < sizeof(ElemT)) {
                    warn_accumulator_overflow<AccumT, ElemT>();
                }
            }
        }

        // body signature concepts
        template<typename F, typename T>
        concept ForBody = std::invocable<F, T>;

        template<typename F, typename T>
        concept ForCtrlBody = std::invocable<F, T, LoopCtrl<void>&>;

        template<typename F, typename T, typename R>
        concept ForRetBody = std::invocable<F, T, LoopCtrl<R>&>;

        template<typename F, typename T>
        concept ForUntypedCtrlBody = std::invocable<F, T, ForCtrl&>;

        template<typename F, typename T, typename R>
        concept ForTypedCtrlBody = std::invocable<F, T, ForCtrlTyped<R>&>;

        template<typename F, typename Ref>
        concept ForRangeBody = std::invocable<F, Ref>;

        template<typename F, typename Ref>
        concept ForRangeCtrlBody = std::invocable<F, Ref, LoopCtrl<void>&>;

        template<typename F, typename Ref, typename R>
        concept ForRangeRetBody = std::invocable<F, Ref, LoopCtrl<R>&>;

        template<typename F, typename Ref>
        concept ForRangeUntypedCtrlBody = std::invocable<F, Ref, ForCtrl&>;

        template<typename F, typename Ref, typename R>
        concept ForRangeTypedCtrlBody = std::invocable<F, Ref, ForCtrlTyped<R>&>;

    } // namespace detail
} // namespace ilp
