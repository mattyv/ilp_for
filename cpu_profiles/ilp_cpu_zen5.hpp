#pragma once

// ILP CPU Profile: AMD Zen 5 (Ryzen 9000 / AI 300 series)
// L1D: 48 KiB 12-way, L2: 1 MiB 16-way, AVX-512
// ROB: 448 entries (vs 320 Zen 4), Dispatch: 8-wide (vs 6-wide Zen 4)
// ALU Scheduler: 88 entries, AGU Scheduler: 56 entries, Int PRF: 240
// Reference: https://www.amd.com/content/dam/amd/en/documents/epyc-business-docs/white-papers/5th-gen-amd-epyc-processor-architecture-white-paper.pdf

#include <cstddef>

// =============================================================================
// Macro definitions for pragma compatibility
// =============================================================================

// Sum - Zen 5 has deep OoO and wide execution, benefits from more ILP
#define ILP_N_SUM_1  16
#define ILP_N_SUM_2  8
#define ILP_N_SUM_4  8
#define ILP_N_SUM_8  8

// DotProduct - wide FMA units benefit from multiple accumulators
#define ILP_N_DOTPRODUCT_4  8
#define ILP_N_DOTPRODUCT_8  8

// Search - 4 works well, good branch prediction handles early exits
#define ILP_N_SEARCH_1  8
#define ILP_N_SEARCH_2  4
#define ILP_N_SEARCH_4  4
#define ILP_N_SEARCH_8  4

// Copy - can push more with improved memory subsystem
#define ILP_N_COPY_1  16
#define ILP_N_COPY_2  8
#define ILP_N_COPY_4  8
#define ILP_N_COPY_8  4

// Transform - good ILP opportunities with wide dispatch
#define ILP_N_TRANSFORM_1  8
#define ILP_N_TRANSFORM_2  8
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

// Template on loop type and element size (bytes)
// Smaller elements benefit from more unrolling (better SIMD packing)

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
