// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <concepts>
#include <cstddef>
#include <ranges>

#include "ctrl.hpp"
#include "mode.hpp"

namespace ilp {
    namespace detail {

        template<typename Range>
        concept SizedRandomAccessRange =
            std::ranges::random_access_range<Range> && std::ranges::sized_range<Range>;

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

        // body signature concepts
        template<typename F, typename T>
        concept ForEachBody = std::invocable<F, T, EachCtrl&>;

        template<typename F, typename T>
        concept ForUntypedCtrlBody = std::invocable<F, T, ForCtrl&>;

        template<typename F, typename T, typename R>
        concept ForTypedCtrlBody = std::invocable<F, T, ForCtrlTyped<R>&>;

        template<typename F, typename Ref>
        concept ForEachRangeBody = std::invocable<F, Ref, EachCtrl&>;

        template<typename F, typename Ref>
        concept ForRangeUntypedCtrlBody = std::invocable<F, Ref, ForCtrl&>;

        template<typename F, typename Ref, typename R>
        concept ForRangeTypedCtrlBody = std::invocable<F, Ref, ForCtrlTyped<R>&>;

    } // namespace detail
} // namespace ilp
