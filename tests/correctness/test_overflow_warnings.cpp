#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <cstdint>

// =============================================================================
// TEST: Overflow Risk Detection
// =============================================================================
// These tests verify that the library warns about potential overflow conditions
// when accumulator types are too small for sum operations.

// -----------------------------------------------------------------------------
// Safe operations - no warnings
// -----------------------------------------------------------------------------

TEST_CASE("Safe: int64_t accumulator for int32_t elements", "[overflow][safe]") {
    std::vector<int32_t> data = {1, 2, 3, 4, 5};

    // This should NOT produce warnings - accumulator is larger
    auto result = ILP_REDUCE_RANGE_SUM(auto&& val, data, 4) {
        return static_cast<int64_t>(val);  // Explicit upcast to larger type
    } ILP_END_REDUCE;

    REQUIRE(result == 15);
}

TEST_CASE("Safe: double accumulator for int elements", "[overflow][safe]") {
    std::vector<int> data = {10, 20, 30, 40};

    // This should NOT produce warnings - floating point has better overflow characteristics
    auto result = ILP_REDUCE_RANGE_SUM(auto&& val, data, 4) {
        return static_cast<double>(val);
    } ILP_END_REDUCE;

    REQUIRE(result == 100.0);
}

TEST_CASE("Safe: int64_t accumulator for range-based sum", "[overflow][safe]") {
    std::vector<int16_t> data = {100, 200, 300, 400, 500};

    auto result = ILP_REDUCE_RANGE_SUM(auto&& val, data, 4) {
        return static_cast<int64_t>(val);
    } ILP_END_REDUCE;

    REQUIRE(result == 1500);
}

// -----------------------------------------------------------------------------
// Potentially unsafe operations - WILL produce deprecation warnings
// -----------------------------------------------------------------------------

TEST_CASE("Unsafe: int accumulator for int elements", "[overflow][unsafe]") {
    std::vector<int> data = {1, 2, 3, 4, 5};

    // WARNING: This will produce a deprecation warning at compile time
    // Accumulator is same size as elements - potential overflow
    auto result = ILP_REDUCE_RANGE_SUM(auto&& val, data, 4) {
        return val;  // Returns int, accumulates into int
    } ILP_END_REDUCE;

    REQUIRE(result == 15);
}

TEST_CASE("Unsafe: int8_t accumulator for int8_t elements", "[overflow][unsafe]") {
    std::vector<int8_t> data = {1, 2, 3, 4, 5};

    // WARNING: This will produce a deprecation warning
    // Very small accumulator - will overflow quickly
    auto result = ILP_REDUCE_RANGE_SUM(auto&& val, data, 4) {
        return val;  // Returns int8_t
    } ILP_END_REDUCE;

    REQUIRE(result == 15);
}

TEST_CASE("Unsafe: int16_t accumulator for int16_t elements", "[overflow][unsafe]") {
    // WARNING: This will produce a deprecation warning
    auto result = ILP_REDUCE_SUM(auto i, 0, 10, 4) {
        return static_cast<int16_t>(i);
    } ILP_END_REDUCE;

    REQUIRE(result == 45);
}

TEST_CASE("Unsafe: uint32_t accumulator for uint32_t range", "[overflow][unsafe]") {
    std::vector<uint32_t> data = {1000000, 2000000, 3000000};

    // WARNING: This will produce a deprecation warning
    // Unsigned overflow is well-defined but likely not intended
    auto result = ILP_REDUCE_RANGE_SUM(auto&& val, data, 4) {
        return val;
    } ILP_END_REDUCE;

    REQUIRE(result == 6000000);
}

// -----------------------------------------------------------------------------
// Edge case: Actual overflow demonstration
// -----------------------------------------------------------------------------

TEST_CASE("Actual overflow: int8_t overflows quickly", "[overflow][demonstration]") {
    std::vector<int8_t> data(200, 1);  // 200 elements, each = 1

    // WARNING: This will produce a deprecation warning AND actually overflow
    // int8_t can only hold -128 to 127, so sum of 200 will overflow
    auto result = ILP_REDUCE_RANGE_SUM(auto&& val, data, 4) {
        return val;
    } ILP_END_REDUCE;

    // Result will be wrong due to overflow (200 % 256 = -56 in signed int8_t)
    // This test documents the behavior but doesn't REQUIRE correctness
    INFO("Result with overflow: " << static_cast<int>(result));
    // Actual result will be incorrect due to overflow
}

TEST_CASE("Correct approach: Use larger accumulator", "[overflow][correct]") {
    std::vector<int8_t> data(200, 1);  // 200 elements, each = 1

    // No warning - using larger accumulator type
    auto result = ILP_REDUCE_RANGE_SUM(auto&& val, data, 4) {
        return static_cast<int32_t>(val);  // Cast to larger type
    } ILP_END_REDUCE;

    REQUIRE(result == 200);  // Correct result
}

// -----------------------------------------------------------------------------
// Mixed scenarios
// -----------------------------------------------------------------------------

TEST_CASE("Step-based sum with potential overflow", "[overflow][unsafe]") {
    // WARNING: This will produce a deprecation warning
    auto result = ILP_REDUCE_STEP_SUM(auto i, 0, 100, 2, 4) {
        return static_cast<int>(i);  // int accumulating int - same size
    } ILP_END_REDUCE;

    REQUIRE(result == 2450);  // 0+2+4+...+98
}

TEST_CASE("Explicit init with sufficient type bypasses issue", "[overflow][workaround]") {
    std::vector<int> data = {1, 2, 3, 4, 5};

    // Using reduce_simple with explicit int64_t init avoids the warning
    auto result = ILP_REDUCE_RANGE_SIMPLE(std::plus<>{}, int64_t{0}, auto&& val, data, 4) {
        return static_cast<int64_t>(val);
    } ILP_END_REDUCE;

    REQUIRE(result == 15);
}

// -----------------------------------------------------------------------------
// Documentation: How to interpret the warnings
// -----------------------------------------------------------------------------

// When you see the deprecation warning:
//   "Overflow risk: accumulator type may be too small for sum."
//
// Solutions:
//   1. Return a larger type from your lambda (e.g., int64_t instead of int32_t)
//   2. Use floating point (e.g., double) if appropriate
//   3. Use ILP_REDUCE_RANGE_SIMPLE with explicit larger init type
//   4. If your range is small and bounded, you can ignore the warning
//
// Example fixes:
//   BAD:  return val;                           // int -> int
//   GOOD: return static_cast<int64_t>(val);    // int -> int64_t
//   GOOD: return static_cast<double>(val);     // int -> double
