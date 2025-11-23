#pragma once

// ILP CPU Profile: Apple M1 (Firestorm P-cores)
//
// Sources:
// - Dougall Johnson: https://dougallj.github.io/applecpu/firestorm.html
// - ocxtal benchmarks: https://github.com/ocxtal/insn_bench_aarch64
//
// Firestorm characteristics:
// - 6 integer ALU units
// - 4 SIMD/FP units
// - 4 load/store units (3 loads/cycle throughput)
// - Integer add: 1c latency, 6/cycle throughput
// - FP add: 2c latency, 4/cycle throughput
// - FP multiply: 3c latency, 4/cycle throughput
// - FMA: 3c latency, 4/cycle throughput
// - L1 load: 3c scalar, 5c SIMD

#include <cstddef>

// =============================================================================
// Macro definitions for pragma compatibility
// =============================================================================

// Sum
#define ILP_N_SUM_1  16
#define ILP_N_SUM_2  8
#define ILP_N_SUM_4  8
#define ILP_N_SUM_8  8

// DotProduct
#define ILP_N_DOTPRODUCT_4  8
#define ILP_N_DOTPRODUCT_8  8

// Search
#define ILP_N_SEARCH_1  8
#define ILP_N_SEARCH_2  4
#define ILP_N_SEARCH_4  4
#define ILP_N_SEARCH_8  4

// Copy
#define ILP_N_COPY_1  16
#define ILP_N_COPY_2  8
#define ILP_N_COPY_4  8
#define ILP_N_COPY_8  4

// Transform
#define ILP_N_TRANSFORM_1  8
#define ILP_N_TRANSFORM_2  8
#define ILP_N_TRANSFORM_4  4
#define ILP_N_TRANSFORM_8  4

namespace ilp {

enum class LoopType {
    Sum,
    DotProduct,
    Search,
    Copy,
    Transform,
};

// Primary template
template<LoopType T, std::size_t ElementBytes = 4>
inline constexpr std::size_t optimal_N = 4;

// Sum: hide latency with multiple accumulators
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 1> = ILP_N_SUM_1;
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 2> = ILP_N_SUM_2;
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 4> = ILP_N_SUM_4;
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 8> = ILP_N_SUM_8;

// DotProduct: FMA latency hiding
template<> inline constexpr std::size_t optimal_N<LoopType::DotProduct, 4> = ILP_N_DOTPRODUCT_4;
template<> inline constexpr std::size_t optimal_N<LoopType::DotProduct, 8> = ILP_N_DOTPRODUCT_8;

// Search: early exit, don't over-unroll
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 1> = ILP_N_SEARCH_1;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 2> = ILP_N_SEARCH_2;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 4> = ILP_N_SEARCH_4;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 8> = ILP_N_SEARCH_8;

// Copy: load/store unit limited
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 1> = ILP_N_COPY_1;
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 2> = ILP_N_COPY_2;
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 4> = ILP_N_COPY_4;
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 8> = ILP_N_COPY_8;

// Transform
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 1> = ILP_N_TRANSFORM_1;
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 2> = ILP_N_TRANSFORM_2;
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 4> = ILP_N_TRANSFORM_4;
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 8> = ILP_N_TRANSFORM_8;

} // namespace ilp
