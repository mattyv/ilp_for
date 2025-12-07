// ilp_for - ILP loop unrolling for C++23
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: MIT

#pragma once

// Intel Alder Lake (Golden Cove P-cores)
// Source: https://uops.info
//
// +----------------+----------+---------+------+-------+
// | Instruction    | Use Case | Latency | RThr | L×TPC |
// +----------------+----------+---------+------+-------+
// | VFMADD231PS/PD | FMA      |    4    | 0.50 |   8   |
// | VADDPS/VADDPD  | FP Add   |    3    | 0.50 |   6   |
// | VPADDB/W/D/Q   | Int Add  |    1    | 0.33 |   3   |
// +----------------+----------+---------+------+-------+

// Sum - Integer (VPADD*): L=1, RThr=0.33, TPC=3 → 1×3 = 3
#define ILP_N_SUM_1 3  // VPADDB
#define ILP_N_SUM_2 3  // VPADDW
#define ILP_N_SUM_4I 3 // VPADDD
#define ILP_N_SUM_8I 3 // VPADDQ

// Sum - Floating Point (VADDPS/VADDPD): L=3, RThr=0.5, TPC=2 → 3×2 = 6
#define ILP_N_SUM_4F 6 // VADDPS
#define ILP_N_SUM_8F 6 // VADDPD

// DotProduct - FMA (VFMADD231PS/PD): L=4, RThr=0.5, TPC=2 → 4×2 = 8
#define ILP_N_DOTPRODUCT_4 8
#define ILP_N_DOTPRODUCT_8 8

// Search - branching loop
#define ILP_N_SEARCH_1 4
#define ILP_N_SEARCH_2 4
#define ILP_N_SEARCH_4 4
#define ILP_N_SEARCH_8 4

// Copy - memory bandwidth limited
#define ILP_N_COPY_1 8
#define ILP_N_COPY_2 4
#define ILP_N_COPY_4 4
#define ILP_N_COPY_8 4

// Transform - memory + compute balanced
#define ILP_N_TRANSFORM_1 4
#define ILP_N_TRANSFORM_2 4
#define ILP_N_TRANSFORM_4 4
#define ILP_N_TRANSFORM_8 4

// Multiply - product reduction (acc *= val)
// VMULPS/PD: L=4, RThr=0.5, TPC=2 → 8
// VPMULLD: L=10, RThr=1.0, TPC=1 → 10
#define ILP_N_MULTIPLY_4F 8  // float: VMULPS
#define ILP_N_MULTIPLY_8F 8  // double: VMULPD
#define ILP_N_MULTIPLY_4I 10 // int32: VPMULLD (high latency!)
#define ILP_N_MULTIPLY_8I 4  // int64: no native

// Divide - VDIVPS/PD: very high latency, low throughput
// VDIVPS: L=11, RThr=5.0, TPC=0.2 → 2
// VDIVPD: L=13, RThr=8.0, TPC=0.125 → 2
#define ILP_N_DIVIDE_4F 2 // float: VDIVPS
#define ILP_N_DIVIDE_8F 2 // double: VDIVPD

// Sqrt - VSQRTPS/PD: very high latency, low throughput
// VSQRTPS: L=12, RThr=6.0, TPC=0.167 → 2
// VSQRTPD: L=13, RThr=9.0, TPC=0.11 → 1
#define ILP_N_SQRT_4F 2 // float: VSQRTPS
#define ILP_N_SQRT_8F 2 // double: VSQRTPD (min 2)

// MinMax - VMINPS/VMAXPS (FP), VPMINS*/VPMAXS* (Int)
// VMINPS: L=4, RThr=0.5, TPC=2 → 8
// VPMINSW: L=1, RThr=0.5, TPC=2 → 2
#define ILP_N_MINMAX_1 2  // int8: VPMINSB
#define ILP_N_MINMAX_2 2  // int16: VPMINSW
#define ILP_N_MINMAX_4I 2 // int32: VPMINSD
#define ILP_N_MINMAX_8I 2 // int64
#define ILP_N_MINMAX_4F 8 // float: VMINPS
#define ILP_N_MINMAX_8F 8 // double: VMINPD

// Bitwise - VPAND/VPOR/VPXOR: L=1, RThr=0.33, TPC=3 → 3
#define ILP_N_BITWISE_1 3
#define ILP_N_BITWISE_2 3
#define ILP_N_BITWISE_4 3
#define ILP_N_BITWISE_8 3

// Shift - VPSLL*/VPSRL*: L=1, RThr=1.0, TPC=1 → 1 (but min 2 for ILP)
#define ILP_N_SHIFT_1 2
#define ILP_N_SHIFT_2 2
#define ILP_N_SHIFT_4 2
#define ILP_N_SHIFT_8 2

// Include the shared computation logic
#include "ilp_optimal_n.hpp"
