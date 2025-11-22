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
// Integer: 1c latency * 4/cycle = 4 accumulators
// FP: 4c latency * 2/cycle = 8 accumulators
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 1> = 16; // int8 - SIMD lanes
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 2> = 8;  // int16
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 4> = 8;  // int32/float - hide FP latency
template<> inline constexpr std::size_t optimal_N<LoopType::Sum, 8> = 8;  // double - 4c latency * 2/cycle

// DotProduct: FMA 4c latency, 2/cycle throughput
// Need 8 accumulators, but 2 loads per iter limits to ~4
template<> inline constexpr std::size_t optimal_N<LoopType::DotProduct, 4> = 8;
template<> inline constexpr std::size_t optimal_N<LoopType::DotProduct, 8> = 8;

// Search: early exit, don't over-unroll
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 1> = 8;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 2> = 4;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 4> = 4;
template<> inline constexpr std::size_t optimal_N<LoopType::Search, 8> = 4;

// Copy: 2 load ports, 1 store port limits throughput
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 1> = 16;
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 2> = 8;
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 4> = 4;
template<> inline constexpr std::size_t optimal_N<LoopType::Copy, 8> = 4;

// Transform: 1 load, 1 op, 1 store per iteration
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 1> = 8;
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 2> = 4;
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 4> = 4;
template<> inline constexpr std::size_t optimal_N<LoopType::Transform, 8> = 4;

} // namespace ilp
