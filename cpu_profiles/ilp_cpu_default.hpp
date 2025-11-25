#pragma once

// ILP CPU Profile: Default (conservative cross-platform values)

#include <cstddef>

// =============================================================================
// Macro definitions for pragma compatibility
// =============================================================================

// Sum
#define ILP_N_SUM_1  16
#define ILP_N_SUM_2  8
#define ILP_N_SUM_4  4
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

// =============================================================================
// Loop Types
// =============================================================================

enum class LoopType {
    Sum,          // sum += arr[i]
    DotProduct,   // sum += a[i] * b[i]
    Search,       // find with early exit
    Copy,         // dst[i] = src[i]
    Transform,    // dst[i] = f(src[i])
};

// =============================================================================
// Optimal Unroll Factors
// =============================================================================
//
// Template on loop type and element size (bytes).
// Smaller elements benefit from more unrolling (better SIMD packing).
//
// Default profile values (conservative cross-platform):
// +-----------+------+------+------+------+
// | LoopType  |  1B  |  2B  |  4B  |  8B  |
// +-----------+------+------+------+------+
// | Sum       |  16  |   8  |   4  |   8  |
// | DotProduct|   -  |   -  |   8  |   8  |
// | Search    |   8  |   4  |   4  |   4  |
// | Copy      |  16  |   8  |   4  |   4  |
// | Transform |   8  |   4  |   4  |   4  |
// +-----------+------+------+------+------+

// Primary template - conservative default
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
