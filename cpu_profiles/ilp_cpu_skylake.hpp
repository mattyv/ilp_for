#pragma once

// ILP CPU Profile: Intel Skylake
// Sources: Agner Fog's instruction tables, uops.info
//
// Skylake characteristics:
// - 4 scalar ALU ports (0, 1, 5, 6)
// - 3 SIMD/FP ports (0, 1, 5)
// - 2 load ports, 1 store port
// - Integer add: 1c latency, 4/cycle throughput
// - FP add/mul: 4c latency, 2/cycle throughput (0.5 CPI)
// - FMA: 4c latency, 2/cycle throughput
// - L1 load: 4-5 cycles

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
#define ILP_N_COPY_4  4
#define ILP_N_COPY_8  4

// Transform
#define ILP_N_TRANSFORM_1  8
#define ILP_N_TRANSFORM_2  4
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

// Sum
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 1> = ILP_N_SUM_1;
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 2> = ILP_N_SUM_2;
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 4> = ILP_N_SUM_4;
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 8> = ILP_N_SUM_8;

// DotProduct
template<> inline constexpr std::size_t optimal_N<LoopType::DotProduct, 4> = ILP_N_DOTPRODUCT_4;
template<> inline constexpr std::size_t optimal_N<LoopType::DotProduct, 8> = ILP_N_DOTPRODUCT_8;

// Search
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 1> = ILP_N_SEARCH_1;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 2> = ILP_N_SEARCH_2;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 4> = ILP_N_SEARCH_4;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 8> = ILP_N_SEARCH_8;

// Copy
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
