#pragma once

// ILP CPU Profile: Apple M1 (Firestorm P-cores)
// Sources: Dougall Johnson's microarchitecture docs, LLVM/XNU sources
//
// Firestorm characteristics:
// - 6 integer ALU units
// - 4 SIMD/FP units
// - 4 load/store units (can do 4 loads or 2 loads + 2 stores)
// - Integer add: 1c latency, 6/cycle throughput
// - FP add: 2c latency, 4/cycle throughput
// - FP multiply: 3c latency, 4/cycle throughput
// - FMA: 4c latency, 4/cycle throughput
// - L1 load: 3c scalar, 5c SIMD

#include <cstddef>

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
// Integer: 1c latency * 6/cycle = 6, round to 8
// FP add: 2c latency * 4/cycle = 8
// FP mul: 3c latency * 4/cycle = 12, use 8
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 1> = 16; // int8 - SIMD lanes
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 2> = 8;  // int16
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 4> = 8;  // int32/float
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 8> = 8;  // double

// DotProduct: FMA 4c latency * 4/cycle = 16, but load-limited
// 4 load ports means we can sustain 2 dot products per cycle
// Use 8 for balance between latency hiding and code size
template<> inline constexpr std::size_t optimal_N<LoopType::DotProduct, 4> = 8;
template<> inline constexpr std::size_t optimal_N<LoopType::DotProduct, 8> = 8;

// Search: early exit, don't over-unroll
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 1> = 8;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 2> = 4;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 4> = 4;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 8> = 4;

// Copy: 4 load/store units, can do more aggressive unrolling
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 1> = 16;
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 2> = 8;
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 4> = 8;  // More aggressive than Skylake
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 8> = 4;

// Transform: with 4 SIMD units, can be more aggressive
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 1> = 8;
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 2> = 8;
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 4> = 4;
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 8> = 4;

} // namespace ilp
