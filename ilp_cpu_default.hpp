#pragma once

// ILP CPU Profile: Default (conservative cross-platform values)

#include <cstddef>

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

// Template on loop type and element size (bytes)
// Smaller elements benefit from more unrolling (better SIMD packing)

// Primary template - conservative default
template<LoopType T, std::size_t ElementBytes = 4>
inline constexpr std::size_t optimal_N = 4;

// Sum: unroll based on element size and latency
// More unrolling for smaller types (better SIMD utilization)
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 1> = 16; // int8
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 2> = 8;  // int16
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 4> = 4;  // int32/float
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 8> = 8;  // int64/double (hide latency)

// DotProduct: 2 loads per iteration, hide FMA latency
template<> inline constexpr std::size_t optimal_N<LoopType::DotProduct, 4> = 8;
template<> inline constexpr std::size_t optimal_N<LoopType::DotProduct, 8> = 8;

// Search: don't over-unroll (early exit)
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 1> = 8;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 2> = 4;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 4> = 4;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 8> = 4;

// Copy: moderate unroll, store port limited
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 1> = 16;
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 2> = 8;
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 4> = 4;
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 8> = 4;

// Transform: 1 load, 1 op, 1 store
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 1> = 8;
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 2> = 4;
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 4> = 4;
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 8> = 4;

} // namespace ilp
