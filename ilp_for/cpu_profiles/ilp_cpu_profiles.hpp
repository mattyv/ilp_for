// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

// Shared CPU profile data for both library and clang-tidy tool.
// This is the single source of truth for N values.

#include <string_view>

namespace ilp::cpu {

    struct Profile {
        // Sum - Integer (VPADD*) and Floating Point (VADDPS/PD)
        int sum_1, sum_2, sum_4i, sum_8i, sum_4f, sum_8f;

        // DotProduct - FMA (VFMADD*)
        int dotproduct_4, dotproduct_8;

        // Search - compare + conditional branch
        int search_1, search_2, search_4, search_8;

        // Copy - memory bandwidth limited
        int copy_1, copy_2, copy_4, copy_8;

        // Transform - memory + compute balanced
        int transform_1, transform_2, transform_4, transform_8;

        // Multiply - product reduction (VMUL*)
        int multiply_4f, multiply_8f, multiply_4i, multiply_8i;

        // Divide - VDIV* (high latency)
        int divide_4f, divide_8f;

        // Sqrt - VSQRT* (high latency)
        int sqrt_4f, sqrt_8f;

        // MinMax - VMIN*/VMAX* and VPMINS*/VPMAXS*
        int minmax_1, minmax_2, minmax_4i, minmax_8i, minmax_4f, minmax_8f;

        // Bitwise - VPAND/VPOR/VPXOR
        int bitwise_1, bitwise_2, bitwise_4, bitwise_8;

        // Shift - VPSLL*/VPSRL*
        int shift_1, shift_2, shift_4, shift_8;
    };

    // Intel Skylake - Source: https://uops.info
    //
    // +----------------+----------+---------+------+-------+
    // | Instruction    | Use Case | Latency | RThr | L×TPC |
    // +----------------+----------+---------+------+-------+
    // | VFMADD231PS/PD | FMA      |    4    | 0.50 |   8   |
    // | VADDPS/VADDPD  | FP Add   |    4    | 0.50 |   8   |
    // | VPADDB/W/D/Q   | Int Add  |    1    | 0.33 |   3   |
    // | VMULPS/VMULPD  | FP Mul   |    4    | 0.50 |   8   |
    // | VPMULLD        | Int Mul  |   10    | 1.00 |  10   |
    // | VDIVPS         | FP Div   |   11    | 3.00 |   4   |
    // | VSQRTPS        | FP Sqrt  |   12    | 3.00 |   4   |
    // | VMINPS/VMAXPS  | FP MinMax|    4    | 0.50 |   8   |
    // | VPMINS*/VPMAXS*| Int MinMax|   1    | 0.33 |   3   |
    // | VPAND/POR/PXOR | Bitwise  |    1    | 0.33 |   3   |
    // | VPSLLW/VPSRLW  | Shift    |    1    | 0.50 |   2   |
    // +----------------+----------+---------+------+-------+
    inline constexpr Profile skylake = {
        // Sum - Integer: L=1, TPC=3 → 3; FP: L=4, TPC=2 → 8
        .sum_1 = 3,
        .sum_2 = 3,
        .sum_4i = 3,
        .sum_8i = 3,
        .sum_4f = 8,
        .sum_8f = 8,
        // DotProduct - FMA: L=4, TPC=2 → 8
        .dotproduct_4 = 8,
        .dotproduct_8 = 8,
        // Search - compare + branch
        .search_1 = 4,
        .search_2 = 4,
        .search_4 = 4,
        .search_8 = 4,
        // Copy - memory bandwidth limited
        .copy_1 = 8,
        .copy_2 = 4,
        .copy_4 = 4,
        .copy_8 = 4,
        // Transform - memory + compute balanced
        .transform_1 = 4,
        .transform_2 = 4,
        .transform_4 = 4,
        .transform_8 = 4,
        // Multiply - FP: L=4, TPC=2 → 8; Int32: L=10, TPC=1 → 10
        .multiply_4f = 8,
        .multiply_8f = 8,
        .multiply_4i = 10,
        .multiply_8i = 4,
        // Divide - high latency: L=11, TPC=0.2 → 2
        .divide_4f = 2,
        .divide_8f = 2,
        // Sqrt - high latency: L=12, TPC=0.17 → 2
        .sqrt_4f = 2,
        .sqrt_8f = 2,
        // MinMax - Int: L=1, TPC=2 → 2; FP: L=4, TPC=2 → 8
        .minmax_1 = 2,
        .minmax_2 = 2,
        .minmax_4i = 2,
        .minmax_8i = 2,
        .minmax_4f = 8,
        .minmax_8f = 8,
        // Bitwise - L=1, TPC=3 → 3
        .bitwise_1 = 3,
        .bitwise_2 = 3,
        .bitwise_4 = 3,
        .bitwise_8 = 3,
        // Shift - L=1, TPC=2 → 2
        .shift_1 = 2,
        .shift_2 = 2,
        .shift_4 = 2,
        .shift_8 = 2,
    };

    // Apple M1 (Firestorm P-cores) - Source: https://dougallj.github.io/applecpu/firestorm.html
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
    inline constexpr Profile apple_m1 = {
        // Sum - Integer: L=2, TPC=4 → 8; FP: L=3, TPC=4 → 12
        .sum_1 = 8,
        .sum_2 = 8,
        .sum_4i = 8,
        .sum_8i = 8,
        .sum_4f = 12,
        .sum_8f = 12,
        // DotProduct - FMA: L=4, TPC=4 → 16
        .dotproduct_4 = 16,
        .dotproduct_8 = 16,
        // Search - FCMP: L=2, TPC=3 → 6; M1's excellent branch predictor allows higher N
        .search_1 = 6,
        .search_2 = 6,
        .search_4 = 6,
        .search_8 = 6,
        // Copy - 4 load/store units
        .copy_1 = 8,
        .copy_2 = 8,
        .copy_4 = 4,
        .copy_8 = 4,
        // Transform - excellent ILP with 4 FP pipes
        .transform_1 = 8,
        .transform_2 = 4,
        .transform_4 = 4,
        .transform_8 = 4,
        // Multiply - FMUL: L=3, TPC=4 → 12
        .multiply_4f = 12,
        .multiply_8f = 12,
        .multiply_4i = 8,
        .multiply_8i = 8,
        // Divide - FDIV: L≈10-14, limited throughput → 4
        .divide_4f = 4,
        .divide_8f = 4,
        // Sqrt - FSQRT: similar to FDIV
        .sqrt_4f = 4,
        .sqrt_8f = 4,
        // MinMax - same execution path as FADD: L=3, TPC=4 → 12; Int: 8
        .minmax_1 = 8,
        .minmax_2 = 8,
        .minmax_4i = 8,
        .minmax_8i = 8,
        .minmax_4f = 12,
        .minmax_8f = 12,
        // Bitwise - AND/ORR/EOR: L=2, TPC=4 → 8
        .bitwise_1 = 8,
        .bitwise_2 = 8,
        .bitwise_4 = 8,
        .bitwise_8 = 8,
        // Shift - SHL/USHR: L=2, TPC=4 → 8
        .shift_1 = 8,
        .shift_2 = 8,
        .shift_4 = 8,
        .shift_8 = 8,
    };

    // Intel Alder Lake (Golden Cove P-cores) - Source: https://uops.info
    //
    // +----------------+----------+---------+------+-------+
    // | Instruction    | Use Case | Latency | RThr | L×TPC |
    // +----------------+----------+---------+------+-------+
    // | VFMADD231PS/PD | FMA      |    4    | 0.50 |   8   |
    // | VADDPS/VADDPD  | FP Add   |    3    | 0.50 |   6   |
    // | VPADDB/W/D/Q   | Int Add  |    1    | 0.33 |   3   |
    // | VUCOMISS/SD    | FP Cmp   |    3    | 1.00 |   3   |
    // | VPCMPEQD       | Int Cmp  |    1    | 0.50 |   2   |
    // | CMP r,r        | Cmp+Flag |    1    | 0.25 |   4   |
    // +----------------+----------+---------+------+-------+
    inline constexpr Profile alderlake = {
        // Sum - Integer: L=1, TPC=3 → 3; FP: L=3, TPC=2 → 6
        .sum_1 = 3,
        .sum_2 = 3,
        .sum_4i = 3,
        .sum_8i = 3,
        .sum_4f = 6,
        .sum_8f = 6,
        // DotProduct - FMA: L=4, TPC=2 → 8
        .dotproduct_4 = 8,
        .dotproduct_8 = 8,
        // Search - compare + branch
        .search_1 = 4,
        .search_2 = 4,
        .search_4 = 4,
        .search_8 = 4,
        // Copy - memory bandwidth limited
        .copy_1 = 8,
        .copy_2 = 4,
        .copy_4 = 4,
        .copy_8 = 4,
        // Transform - memory + compute balanced
        .transform_1 = 4,
        .transform_2 = 4,
        .transform_4 = 4,
        .transform_8 = 4,
        // Multiply - FP: L=4, TPC=2 → 8; Int32: L=10, TPC=1 → 10
        .multiply_4f = 8,
        .multiply_8f = 8,
        .multiply_4i = 10,
        .multiply_8i = 4,
        // Divide - VDIVPS: L=11, RThr=5.0, TPC=0.2 → 2
        .divide_4f = 2,
        .divide_8f = 2,
        // Sqrt - VSQRTPS: L=12, RThr=6.0, TPC=0.167 → 2
        .sqrt_4f = 2,
        .sqrt_8f = 2,
        // MinMax - Int: L=1, TPC=2 → 2; FP: L=4, TPC=2 → 8
        .minmax_1 = 2,
        .minmax_2 = 2,
        .minmax_4i = 2,
        .minmax_8i = 2,
        .minmax_4f = 8,
        .minmax_8f = 8,
        // Bitwise - L=1, TPC=3 → 3
        .bitwise_1 = 3,
        .bitwise_2 = 3,
        .bitwise_4 = 3,
        .bitwise_8 = 3,
        // Shift - L=1, TPC=1 → 2 (min for ILP)
        .shift_1 = 2,
        .shift_2 = 2,
        .shift_4 = 2,
        .shift_8 = 2,
    };

    // AMD Zen 4/5 (Ryzen 7000/9000 series) - Source: https://uops.info
    //
    // +----------------+----------+---------+------+-------+
    // | Instruction    | Use Case | Latency | RThr | L×TPC |
    // +----------------+----------+---------+------+-------+
    // | VFMADD231PS/PD | FMA      |    4    | 0.50 |   8   |
    // | VADDPS/VADDPD  | FP Add   |    3    | 0.50 |   6   |
    // | VPADDB/W/D/Q   | Int Add  |    1    | 0.25 |   4   |
    // | VUCOMISS/SD    | FP Cmp   |    6    | 1.00 |   6   |
    // | VPCMPEQD       | Int Cmp  |    1    | 0.50 |   2   |
    // | CMP r,r        | Cmp+Flag |    1    | 0.25 |   4   |
    // +----------------+----------+---------+------+-------+
    inline constexpr Profile zen5 = {
        // Sum - Integer: L=1, TPC=4 → 4; FP: L=3, TPC=2 → 6
        .sum_1 = 4,
        .sum_2 = 4,
        .sum_4i = 4,
        .sum_8i = 4,
        .sum_4f = 6,
        .sum_8f = 6,
        // DotProduct - FMA: L=4, TPC=2 → 8
        .dotproduct_4 = 8,
        .dotproduct_8 = 8,
        // Search - compare + branch
        .search_1 = 4,
        .search_2 = 4,
        .search_4 = 4,
        .search_8 = 4,
        // Copy - improved memory subsystem
        .copy_1 = 8,
        .copy_2 = 4,
        .copy_4 = 4,
        .copy_8 = 4,
        // Transform - wide dispatch benefits ILP
        .transform_1 = 4,
        .transform_2 = 4,
        .transform_4 = 4,
        .transform_8 = 4,
        // Multiply - VMULPS: L=3, TPC=2 → 6; VPMULLD: L=3, TPC=2 → 6 (much faster than Intel!)
        .multiply_4f = 6,
        .multiply_8f = 6,
        .multiply_4i = 6,
        .multiply_8i = 6,
        // Divide - VDIVPS: L=11, TPC=0.33 → 4; VDIVPD: L=13, TPC=0.2 → 3
        .divide_4f = 4,
        .divide_8f = 3,
        // Sqrt - VSQRTPS: L=15, TPC=0.2 → 3; VSQRTPD: L=21, TPC=0.12 → 3
        .sqrt_4f = 3,
        .sqrt_8f = 3,
        // MinMax - VMINPS: L=2, TPC=2 → 4; VPMINSW: L=1, TPC=4 → 4
        .minmax_1 = 4,
        .minmax_2 = 4,
        .minmax_4i = 4,
        .minmax_8i = 4,
        .minmax_4f = 4,
        .minmax_8f = 4,
        // Bitwise - L=1, TPC=4 → 4
        .bitwise_1 = 4,
        .bitwise_2 = 4,
        .bitwise_4 = 4,
        .bitwise_8 = 4,
        // Shift - L=2, TPC=2 → 4
        .shift_1 = 4,
        .shift_2 = 4,
        .shift_4 = 4,
        .shift_8 = 4,
    };

    // Default - conservative cross-platform values
    inline constexpr Profile default_profile = {
        // Sum - Integer: conservative L=1, TPC=4 → 4; FP: L=4, TPC=2 → 8
        .sum_1 = 4,
        .sum_2 = 4,
        .sum_4i = 4,
        .sum_8i = 4,
        .sum_4f = 8,
        .sum_8f = 8,
        // DotProduct - FMA: conservative L=4, TPC=2 → 8
        .dotproduct_4 = 8,
        .dotproduct_8 = 8,
        // Search - conservative N=4 balances ILP benefit vs misprediction cost
        .search_1 = 4,
        .search_2 = 4,
        .search_4 = 4,
        .search_8 = 4,
        // Copy - memory bandwidth limited
        .copy_1 = 8,
        .copy_2 = 4,
        .copy_4 = 4,
        .copy_8 = 4,
        // Transform - memory + compute
        .transform_1 = 4,
        .transform_2 = 4,
        .transform_4 = 4,
        .transform_8 = 4,
        // Multiply - FP: L=4, TPC=2 → 8; Int: conservative 8
        .multiply_4f = 8,
        .multiply_8f = 8,
        .multiply_4i = 8,
        .multiply_8i = 8,
        // Divide - conservative
        .divide_4f = 4,
        .divide_8f = 4,
        // Sqrt - conservative
        .sqrt_4f = 4,
        .sqrt_8f = 4,
        // MinMax - Int: 4; FP: 8
        .minmax_1 = 4,
        .minmax_2 = 4,
        .minmax_4i = 4,
        .minmax_8i = 4,
        .minmax_4f = 8,
        .minmax_8f = 8,
        // Bitwise - L=1, TPC=3 → 4 (conservative)
        .bitwise_1 = 4,
        .bitwise_2 = 4,
        .bitwise_4 = 4,
        .bitwise_8 = 4,
        // Shift - L=1, TPC=2 → 2
        .shift_1 = 2,
        .shift_2 = 2,
        .shift_4 = 2,
        .shift_8 = 2,
    };

    // Runtime profile lookup by name
    inline const Profile& get(std::string_view name) {
        if (name == "apple_m1" || name == "m1")
            return apple_m1;
        if (name == "alderlake" || name == "alder_lake")
            return alderlake;
        if (name == "zen5" || name == "zen4" || name == "zen")
            return zen5;
        if (name == "default")
            return default_profile;
        return skylake; // default fallback
    }

} // namespace ilp::cpu
