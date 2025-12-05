#pragma once

// ILP CPU Profile: AMD Zen 4/5 (Ryzen 7000/9000 series)
//
// Formula: optimal_N = Latency × TPC (Throughput Per Cycle)
// Source: https://uops.info, https://www.agner.org/optimize/instruction_tables.pdf
//
// Zen 4/5 characteristics:
// - 6-wide decode, 8-wide dispatch (Zen 5)
// - 4 FP/SIMD pipes with excellent throughput
// - ROB: 320 (Zen 4), 448 (Zen 5)
//
// Instruction metrics (AVX2 YMM registers):
// +----------------+----------+---------+------+-------+
// | Instruction    | Use Case | Latency | RThr | L×TPC |
// +----------------+----------+---------+------+-------+
// | VFMADD231PS/PD | FMA      |    4    | 0.50 |   8   |
// | VADDPS/VADDPD  | FP Add   |    3    | 0.50 |   6   |
// | VPADDB/W/D/Q   | Int Add  |    1    | 0.25 |   4   |
// +----------------+----------+---------+------+-------+

// =============================================================================
// Macro definitions (must be defined before including ilp_optimal_n.hpp)
// =============================================================================

// Sum - Integer (VPADD*): L=1, RThr=0.25, TPC=4 → 1×4 = 4
#define ILP_N_SUM_1   4   // VPADDB
#define ILP_N_SUM_2   4   // VPADDW
#define ILP_N_SUM_4I  4   // VPADDD
#define ILP_N_SUM_8I  4   // VPADDQ

// Sum - Floating Point (VADDPS/VADDPD): L=3, RThr=0.5, TPC=2 → 3×2 = 6
#define ILP_N_SUM_4F  6   // VADDPS
#define ILP_N_SUM_8F  6   // VADDPD

// DotProduct - FMA (VFMADD231PS/PD): L=4, RThr=0.5, TPC=2 → 4×2 = 8
#define ILP_N_DOTPRODUCT_4  8
#define ILP_N_DOTPRODUCT_8  8

// Search - branching loop, good branch prediction
#define ILP_N_SEARCH_1  4
#define ILP_N_SEARCH_2  4
#define ILP_N_SEARCH_4  4
#define ILP_N_SEARCH_8  4

// Copy - improved memory subsystem in Zen 4/5
#define ILP_N_COPY_1  8
#define ILP_N_COPY_2  4
#define ILP_N_COPY_4  4
#define ILP_N_COPY_8  4

// Transform - wide dispatch benefits ILP
#define ILP_N_TRANSFORM_1  4
#define ILP_N_TRANSFORM_2  4
#define ILP_N_TRANSFORM_4  4
#define ILP_N_TRANSFORM_8  4

// -----------------------------------------------------------------------------
// New execution unit operations (verified from uops.info - Zen 4)
// -----------------------------------------------------------------------------

// Multiply - product reduction (acc *= val)
// VMULPS: L=3, RThr=0.5, TPC=2 → 6
// VPMULLD: L=3, RThr=0.5, TPC=2 → 6 (much faster than Intel!)
#define ILP_N_MULTIPLY_4F  6    // float: VMULPS
#define ILP_N_MULTIPLY_8F  6    // double: VMULPD
#define ILP_N_MULTIPLY_4I  6    // int32: VPMULLD
#define ILP_N_MULTIPLY_8I  6    // int64

// Divide - VDIVPS/PD: high latency but better throughput than Intel
// VDIVPS: L=11, RThr=3.0, TPC=0.33 → 4
// VDIVPD: L=13, RThr=5.0, TPC=0.2 → 3
#define ILP_N_DIVIDE_4F  4   // float: VDIVPS
#define ILP_N_DIVIDE_8F  3   // double: VDIVPD

// Sqrt - VSQRTPS/PD: high latency
// VSQRTPS: L=15, RThr=5.0, TPC=0.2 → 3
// VSQRTPD: L=21, RThr=8.4, TPC=0.12 → 3
#define ILP_N_SQRT_4F  3   // float: VSQRTPS
#define ILP_N_SQRT_8F  3   // double: VSQRTPD

// MinMax - VMINPS/VMAXPS (FP), VPMINS*/VPMAXS* (Int)
// VMINPS: L=2, RThr=0.5, TPC=2 → 4
// VPMINSW: L=1, RThr=0.25, TPC=4 → 4
#define ILP_N_MINMAX_1   4   // int8: VPMINSB
#define ILP_N_MINMAX_2   4   // int16: VPMINSW
#define ILP_N_MINMAX_4I  4   // int32: VPMINSD
#define ILP_N_MINMAX_8I  4   // int64
#define ILP_N_MINMAX_4F  4   // float: VMINPS
#define ILP_N_MINMAX_8F  4   // double: VMINPD

// Bitwise - VPAND/VPOR/VPXOR: L=1, RThr=0.25, TPC=4 → 4
#define ILP_N_BITWISE_1  4
#define ILP_N_BITWISE_2  4
#define ILP_N_BITWISE_4  4
#define ILP_N_BITWISE_8  4

// Shift - VPSLL*/VPSRL*: L=2, RThr=0.5, TPC=2 → 4
#define ILP_N_SHIFT_1  4
#define ILP_N_SHIFT_2  4
#define ILP_N_SHIFT_4  4
#define ILP_N_SHIFT_8  4

// Include the shared computation logic
#include "ilp_optimal_n.hpp"
