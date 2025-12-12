// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

// Apple M1 (Firestorm P-cores)
// Source: https://dougallj.github.io/applecpu/firestorm.html
//
// +----------------+----------+---------+------+-------+
// | Instruction    | Use Case | Latency | RThr | L×TPC |
// +----------------+----------+---------+------+-------+
// | FMLA           | FMA      |    4    | 0.25 |  16   |
// | FADD           | FP Add   |    3    | 0.25 |  12   |
// | ADD (vec)      | Int Add  |    2    | 0.25 |   8   |
// | FCMP           | FP Cmp   |    2    | 0.33 |   6   |
// | CMP (vec)      | Int Cmp  |    1    | 0.25 |   4   |
// +----------------+----------+---------+------+-------+

// Sum - Integer (ADD vec): L=2, RThr=0.25, TPC=4 → 2×4 = 8
#define ILP_N_SUM_1 8  // ADD 8B/16B
#define ILP_N_SUM_2 8  // ADD 4H/8H
#define ILP_N_SUM_4I 8 // ADD 2S/4S
#define ILP_N_SUM_8I 8 // ADD 2D

// Sum - Floating Point (FADD): L=3, RThr=0.25, TPC=4 → 3×4 = 12
#define ILP_N_SUM_4F 12 // FADD 4S
#define ILP_N_SUM_8F 12 // FADD 2D

// DotProduct - FMA (FMLA): L=4, RThr=0.25, TPC=4 → 4×4 = 16
#define ILP_N_DOTPRODUCT_4 16
#define ILP_N_DOTPRODUCT_8 16

// Search - compare + conditional branch loop
// Unlike arithmetic ops, Search N is constrained by:
// 1. Compare latency (FCMP: L=2, CMP vec: L=1)
// 2. Branch misprediction penalty (~14 cycles on M1)
// 3. Wasted work on early exit (fewer iterations = less waste)
// FCMP: L=2, TPC=3 → 6; M1's excellent branch predictor allows higher N
#define ILP_N_SEARCH_1 6
#define ILP_N_SEARCH_2 6
#define ILP_N_SEARCH_4 6
#define ILP_N_SEARCH_8 6

// Copy - 4 load/store units
#define ILP_N_COPY_1 8
#define ILP_N_COPY_2 8
#define ILP_N_COPY_4 4
#define ILP_N_COPY_8 4

// Transform - excellent ILP with 4 FP pipes
#define ILP_N_TRANSFORM_1 8
#define ILP_N_TRANSFORM_2 4
#define ILP_N_TRANSFORM_4 4
#define ILP_N_TRANSFORM_8 4

// M1 has 4 SIMD/FP units → exceptional TPC of 4

// Multiply - product reduction (acc *= val)
// FMUL: L=3, RThr=0.25, TPC=4 → 12 (same execution path as FADD)
#define ILP_N_MULTIPLY_4F 12 // float: FMUL 4S
#define ILP_N_MULTIPLY_8F 12 // double: FMUL 2D
#define ILP_N_MULTIPLY_4I 8  // int32: MUL vec
#define ILP_N_MULTIPLY_8I 8  // int64

// Divide - FDIV: high latency, dedicated unit
// FDIV: L≈10-14, limited throughput → conservative 4
#define ILP_N_DIVIDE_4F 4 // float: FDIV
#define ILP_N_DIVIDE_8F 4 // double: FDIV

// Sqrt - FSQRT: high latency, shared with divide unit
// FSQRT: similar to FDIV
#define ILP_N_SQRT_4F 4 // float: FSQRT
#define ILP_N_SQRT_8F 4 // double: FSQRT

// MinMax - FMIN/FMAX (FP), SMIN/SMAX/UMIN/UMAX (Int)
// Same execution path as FADD: L=3, TPC=4 → 12
#define ILP_N_MINMAX_1 8   // int8
#define ILP_N_MINMAX_2 8   // int16
#define ILP_N_MINMAX_4I 8  // int32
#define ILP_N_MINMAX_8I 8  // int64
#define ILP_N_MINMAX_4F 12 // float: FMIN/FMAX
#define ILP_N_MINMAX_8F 12 // double: FMIN/FMAX

// Bitwise - AND/ORR/EOR: L=2, RThr=0.25, TPC=4 → 8
#define ILP_N_BITWISE_1 8
#define ILP_N_BITWISE_2 8
#define ILP_N_BITWISE_4 8
#define ILP_N_BITWISE_8 8

// Shift - SHL/USHR: L=2, RThr=0.25, TPC=4 → 8
#define ILP_N_SHIFT_1 8
#define ILP_N_SHIFT_2 8
#define ILP_N_SHIFT_4 8
#define ILP_N_SHIFT_8 8

// Include the shared computation logic
#include "ilp_optimal_n.hpp"
