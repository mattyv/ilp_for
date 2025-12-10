// ilp_for - ILP loop unrolling for C++23
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

// default profile - conservative cross-platform values
//
// Formula: optimal_N = Latency × TPC
// Sources:
// - x86: https://uops.info, https://www.agner.org/optimize/instruction_tables.pdf
// - ARM: https://dougallj.github.io/applecpu/firestorm.html

// Sum - Integer (VPADD* or ADD vec): conservative L=1, TPC=4 → 4
#define ILP_N_SUM_1 4  // int8
#define ILP_N_SUM_2 4  // int16
#define ILP_N_SUM_4I 4 // int32
#define ILP_N_SUM_8I 4 // int64

// Sum - Floating Point (VADDPS/VADDPD or FADD): conservative L=4, TPC=2 → 8
#define ILP_N_SUM_4F 8 // float
#define ILP_N_SUM_8F 8 // double

// DotProduct - FMA: conservative L=4, TPC=2 → 8
#define ILP_N_DOTPRODUCT_4 8
#define ILP_N_DOTPRODUCT_8 8

// Search - branching loop, don't over-unroll
#define ILP_N_SEARCH_1 4
#define ILP_N_SEARCH_2 4
#define ILP_N_SEARCH_4 4
#define ILP_N_SEARCH_8 4

// Copy - memory bandwidth limited
#define ILP_N_COPY_1 8
#define ILP_N_COPY_2 4
#define ILP_N_COPY_4 4
#define ILP_N_COPY_8 4

// Transform - memory + compute
#define ILP_N_TRANSFORM_1 4
#define ILP_N_TRANSFORM_2 4
#define ILP_N_TRANSFORM_4 4
#define ILP_N_TRANSFORM_8 4

// Multiply - product reduction (acc *= val)
// FP: VMULPS/PD L=4, TPC=2 → 8; Int: VPMULLD L=10, TPC=1 → 10
#define ILP_N_MULTIPLY_4F 8 // float
#define ILP_N_MULTIPLY_8F 8 // double
#define ILP_N_MULTIPLY_4I 8 // int32 (conservative, actual may be 10)
#define ILP_N_MULTIPLY_8I 8 // int64

// Divide - VDIVPS/PD: very high latency (L=11-14), low throughput (TPC≈0.3)
#define ILP_N_DIVIDE_4F 4 // float
#define ILP_N_DIVIDE_8F 4 // double

// Sqrt - VSQRTPS/PD: very high latency (L=12-18), low throughput
#define ILP_N_SQRT_4F 4 // float
#define ILP_N_SQRT_8F 4 // double

// MinMax - VMINPS/PD (FP like add), VPMINS* (Int fast)
#define ILP_N_MINMAX_1 4  // int8
#define ILP_N_MINMAX_2 4  // int16
#define ILP_N_MINMAX_4I 4 // int32
#define ILP_N_MINMAX_8I 4 // int64
#define ILP_N_MINMAX_4F 8 // float
#define ILP_N_MINMAX_8F 8 // double

// Bitwise - VPAND/POR/PXOR: L=1, TPC=3 → 3
#define ILP_N_BITWISE_1 4
#define ILP_N_BITWISE_2 4
#define ILP_N_BITWISE_4 4
#define ILP_N_BITWISE_8 4

// Shift - VPSLL/SRL: L=1, TPC=2 → 2
#define ILP_N_SHIFT_1 2
#define ILP_N_SHIFT_2 2
#define ILP_N_SHIFT_4 2
#define ILP_N_SHIFT_8 2

// Include the shared computation logic
#include "ilp_optimal_n.hpp"
