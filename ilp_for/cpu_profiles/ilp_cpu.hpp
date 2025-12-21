// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

// Single header for CPU profile selection
// Define ILP_CPU_SKYLAKE, ILP_CPU_ALDERLAKE, ILP_CPU_ZEN5, ILP_CPU_APPLE_M1
// before including this header. Default is conservative cross-platform values.

#include "ilp_cpu_profiles.hpp"

// Select profile based on preprocessor define
// Supports both uppercase (ILP_CPU_SKYLAKE) and lowercase (ILP_CPU_skylake)
#if defined(ILP_CPU_SKYLAKE) || defined(ILP_CPU_skylake)
#define ILP_CPU_PROFILE ilp::cpu::skylake
#elif defined(ILP_CPU_ALDERLAKE) || defined(ILP_CPU_alderlake)
#define ILP_CPU_PROFILE ilp::cpu::alderlake
#elif defined(ILP_CPU_ZEN5) || defined(ILP_CPU_zen5) || defined(ILP_CPU_ZEN4) || defined(ILP_CPU_zen4) ||              \
    defined(ILP_CPU_ZEN) || defined(ILP_CPU_zen)
#define ILP_CPU_PROFILE ilp::cpu::zen5
#elif defined(ILP_CPU_APPLE_M1) || defined(ILP_CPU_apple_m1) || defined(ILP_CPU_M1) || defined(ILP_CPU_m1)
#define ILP_CPU_PROFILE ilp::cpu::apple_m1
#elif defined(ILP_CPU_DEFAULT) || defined(ILP_CPU_default)
#define ILP_CPU_PROFILE ilp::cpu::default_profile
#else
#define ILP_CPU_PROFILE ilp::cpu::default_profile
#endif

// Sum
#define ILP_N_SUM_1 ILP_CPU_PROFILE.sum_1
#define ILP_N_SUM_2 ILP_CPU_PROFILE.sum_2
#define ILP_N_SUM_4I ILP_CPU_PROFILE.sum_4i
#define ILP_N_SUM_8I ILP_CPU_PROFILE.sum_8i
#define ILP_N_SUM_4F ILP_CPU_PROFILE.sum_4f
#define ILP_N_SUM_8F ILP_CPU_PROFILE.sum_8f

// DotProduct
#define ILP_N_DOTPRODUCT_4 ILP_CPU_PROFILE.dotproduct_4
#define ILP_N_DOTPRODUCT_8 ILP_CPU_PROFILE.dotproduct_8

// Search
#define ILP_N_SEARCH_1 ILP_CPU_PROFILE.search_1
#define ILP_N_SEARCH_2 ILP_CPU_PROFILE.search_2
#define ILP_N_SEARCH_4 ILP_CPU_PROFILE.search_4
#define ILP_N_SEARCH_8 ILP_CPU_PROFILE.search_8

// Copy
#define ILP_N_COPY_1 ILP_CPU_PROFILE.copy_1
#define ILP_N_COPY_2 ILP_CPU_PROFILE.copy_2
#define ILP_N_COPY_4 ILP_CPU_PROFILE.copy_4
#define ILP_N_COPY_8 ILP_CPU_PROFILE.copy_8

// Transform
#define ILP_N_TRANSFORM_1 ILP_CPU_PROFILE.transform_1
#define ILP_N_TRANSFORM_2 ILP_CPU_PROFILE.transform_2
#define ILP_N_TRANSFORM_4 ILP_CPU_PROFILE.transform_4
#define ILP_N_TRANSFORM_8 ILP_CPU_PROFILE.transform_8

// Multiply
#define ILP_N_MULTIPLY_4F ILP_CPU_PROFILE.multiply_4f
#define ILP_N_MULTIPLY_8F ILP_CPU_PROFILE.multiply_8f
#define ILP_N_MULTIPLY_4I ILP_CPU_PROFILE.multiply_4i
#define ILP_N_MULTIPLY_8I ILP_CPU_PROFILE.multiply_8i

// Divide
#define ILP_N_DIVIDE_4F ILP_CPU_PROFILE.divide_4f
#define ILP_N_DIVIDE_8F ILP_CPU_PROFILE.divide_8f

// Sqrt
#define ILP_N_SQRT_4F ILP_CPU_PROFILE.sqrt_4f
#define ILP_N_SQRT_8F ILP_CPU_PROFILE.sqrt_8f

// MinMax
#define ILP_N_MINMAX_1 ILP_CPU_PROFILE.minmax_1
#define ILP_N_MINMAX_2 ILP_CPU_PROFILE.minmax_2
#define ILP_N_MINMAX_4I ILP_CPU_PROFILE.minmax_4i
#define ILP_N_MINMAX_8I ILP_CPU_PROFILE.minmax_8i
#define ILP_N_MINMAX_4F ILP_CPU_PROFILE.minmax_4f
#define ILP_N_MINMAX_8F ILP_CPU_PROFILE.minmax_8f

// Bitwise
#define ILP_N_BITWISE_1 ILP_CPU_PROFILE.bitwise_1
#define ILP_N_BITWISE_2 ILP_CPU_PROFILE.bitwise_2
#define ILP_N_BITWISE_4 ILP_CPU_PROFILE.bitwise_4
#define ILP_N_BITWISE_8 ILP_CPU_PROFILE.bitwise_8

// Shift
#define ILP_N_SHIFT_1 ILP_CPU_PROFILE.shift_1
#define ILP_N_SHIFT_2 ILP_CPU_PROFILE.shift_2
#define ILP_N_SHIFT_4 ILP_CPU_PROFILE.shift_4
#define ILP_N_SHIFT_8 ILP_CPU_PROFILE.shift_8

// Include the shared computation logic
#include "ilp_optimal_n.hpp"
