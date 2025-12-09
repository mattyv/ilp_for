// ilp_for - ILP loop unrolling for C++23
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

// optimal_N computation from CPU profile macros

#include <cstddef>
#include <type_traits>

#ifndef ILP_N_SUM_4
#define ILP_N_SUM_4 ILP_N_SUM_4F
#endif
#ifndef ILP_N_SUM_8
#define ILP_N_SUM_8 ILP_N_SUM_8F
#endif

namespace ilp {

    enum class LoopType {
        Sum,        // acc += val (VADD)
        DotProduct, // acc += a * b (VFMA)
        Search,     // find with early exit
        Copy,       // dst = src (Load/Store)
        Transform,  // dst = f(src)
        Multiply,   // acc *= val (VMUL)
        Divide,     // val / const (VDIV)
        Sqrt,       // sqrt(val) (VSQRT)
        MinMax,     // acc = min/max(acc, val)
        Bitwise,    // acc &= val, |=, ^=
        Shift,      // val << n, val >> n
    };

    namespace detail {

        template<LoopType L, typename T>
        constexpr std::size_t compute_optimal_N() {
            constexpr std::size_t size = sizeof(T);
            constexpr bool is_fp = std::is_floating_point_v<T>;

            if constexpr (L == LoopType::Sum) {
                if constexpr (size == 1)
                    return ILP_N_SUM_1;
                else if constexpr (size == 2)
                    return ILP_N_SUM_2;
                else if constexpr (size == 4 && is_fp)
                    return ILP_N_SUM_4F;
                else if constexpr (size == 4)
                    return ILP_N_SUM_4I;
                else if constexpr (size == 8 && is_fp)
                    return ILP_N_SUM_8F;
                else if constexpr (size == 8)
                    return ILP_N_SUM_8I;
                else
                    return 4;
            } else if constexpr (L == LoopType::DotProduct) {
                if constexpr (size == 4)
                    return ILP_N_DOTPRODUCT_4;
                else if constexpr (size == 8)
                    return ILP_N_DOTPRODUCT_8;
                else
                    return 4;
            } else if constexpr (L == LoopType::Search) {
                if constexpr (size == 1)
                    return ILP_N_SEARCH_1;
                else if constexpr (size == 2)
                    return ILP_N_SEARCH_2;
                else if constexpr (size == 4)
                    return ILP_N_SEARCH_4;
                else if constexpr (size == 8)
                    return ILP_N_SEARCH_8;
                else
                    return 4;
            } else if constexpr (L == LoopType::Copy) {
                if constexpr (size == 1)
                    return ILP_N_COPY_1;
                else if constexpr (size == 2)
                    return ILP_N_COPY_2;
                else if constexpr (size == 4)
                    return ILP_N_COPY_4;
                else if constexpr (size == 8)
                    return ILP_N_COPY_8;
                else
                    return 4;
            } else if constexpr (L == LoopType::Transform) {
                if constexpr (size == 1)
                    return ILP_N_TRANSFORM_1;
                else if constexpr (size == 2)
                    return ILP_N_TRANSFORM_2;
                else if constexpr (size == 4)
                    return ILP_N_TRANSFORM_4;
                else if constexpr (size == 8)
                    return ILP_N_TRANSFORM_8;
                else
                    return 4;
            } else if constexpr (L == LoopType::Multiply) {
                // Product reduction: acc *= val
                // FP: VMULPS/PD, Int: VPMULLD/Q (int multiply has high latency!)
                if constexpr (size == 4 && is_fp)
                    return ILP_N_MULTIPLY_4F;
                else if constexpr (size == 4)
                    return ILP_N_MULTIPLY_4I;
                else if constexpr (size == 8 && is_fp)
                    return ILP_N_MULTIPLY_8F;
                else if constexpr (size == 8)
                    return ILP_N_MULTIPLY_8I;
                else
                    return 4;
            } else if constexpr (L == LoopType::Divide) {
                // Division: VDIVPS/PD - very high latency, low throughput
                if constexpr (size == 4)
                    return ILP_N_DIVIDE_4F;
                else if constexpr (size == 8)
                    return ILP_N_DIVIDE_8F;
                else
                    return 4;
            } else if constexpr (L == LoopType::Sqrt) {
                // Square root: VSQRTPS/PD - very high latency, low throughput
                if constexpr (size == 4)
                    return ILP_N_SQRT_4F;
                else if constexpr (size == 8)
                    return ILP_N_SQRT_8F;
                else
                    return 4;
            } else if constexpr (L == LoopType::MinMax) {
                // Min/Max reduction: VMINPS/PD, VPMINS*
                if constexpr (size == 1)
                    return ILP_N_MINMAX_1;
                else if constexpr (size == 2)
                    return ILP_N_MINMAX_2;
                else if constexpr (size == 4 && is_fp)
                    return ILP_N_MINMAX_4F;
                else if constexpr (size == 4)
                    return ILP_N_MINMAX_4I;
                else if constexpr (size == 8 && is_fp)
                    return ILP_N_MINMAX_8F;
                else if constexpr (size == 8)
                    return ILP_N_MINMAX_8I;
                else
                    return 4;
            } else if constexpr (L == LoopType::Bitwise) {
                // Bitwise ops: VPAND/POR/PXOR - very fast, 3 ports
                if constexpr (size == 1)
                    return ILP_N_BITWISE_1;
                else if constexpr (size == 2)
                    return ILP_N_BITWISE_2;
                else if constexpr (size == 4)
                    return ILP_N_BITWISE_4;
                else if constexpr (size == 8)
                    return ILP_N_BITWISE_8;
                else
                    return 4;
            } else if constexpr (L == LoopType::Shift) {
                // Shift ops: VPSLL/SRL - 2 ports
                if constexpr (size == 1)
                    return ILP_N_SHIFT_1;
                else if constexpr (size == 2)
                    return ILP_N_SHIFT_2;
                else if constexpr (size == 4)
                    return ILP_N_SHIFT_4;
                else if constexpr (size == 8)
                    return ILP_N_SHIFT_8;
                else
                    return 4;
            } else {
                return 4;
            }
        }

    } // namespace detail

    // Primary template: type-aware optimal_N
    template<LoopType L, typename T>
    inline constexpr std::size_t optimal_N = detail::compute_optimal_N<L, T>();

} // namespace ilp
