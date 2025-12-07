// Suppress deprecation warnings for this entire test file
// (warnings are intentionally triggered to test overflow detection)
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <cstdint>
#include <vector>

#if !defined(ILP_MODE_SIMPLE)

// =============================================================================
// TEST: Overflow Risk Detection
// =============================================================================
// The library warns when accumulator type is STRICTLY SMALLER than element type.
// Same-size accumulation is allowed - users know their data.
//
// Warning condition: sizeof(AccumT) < sizeof(ElemT)

// -----------------------------------------------------------------------------
// Safe operations - no warnings (accumulator >= element size)
// -----------------------------------------------------------------------------

TEST_CASE("Safe: int64_t accumulator for int32_t elements", "[overflow][safe]") {
    std::vector<int32_t> data = {1, 2, 3, 4, 5};

    // No warning - accumulator (8 bytes) > element (4 bytes)
    auto result =
        ilp::reduce_range<4>(data, int64_t{0}, std::plus<>{}, [](auto&& val) { return static_cast<int64_t>(val); });

    REQUIRE(result == 15);
}

TEST_CASE("Safe: double accumulator for int elements", "[overflow][safe]") {
    std::vector<int> data = {10, 20, 30, 40};

    // No warning - floating point (not checked)
    auto result = ilp::reduce_range<4>(data, 0.0, std::plus<>{}, [](auto&& val) { return static_cast<double>(val); });

    REQUIRE(result == 100.0);
}

TEST_CASE("Safe: int64_t accumulator for int16_t elements", "[overflow][safe]") {
    std::vector<int16_t> data = {100, 200, 300, 400, 500};

    // No warning - accumulator (8 bytes) > element (2 bytes)
    auto result =
        ilp::reduce_range<4>(data, int64_t{0}, std::plus<>{}, [](auto&& val) { return static_cast<int64_t>(val); });

    REQUIRE(result == 1500);
}

TEST_CASE("Safe: int32_t accumulator for int8_t elements", "[overflow][safe]") {
    std::vector<int8_t> data(200, 1);

    // No warning - accumulator (4 bytes) > element (1 byte)
    auto result =
        ilp::reduce_range<4>(data, int32_t{0}, std::plus<>{}, [](auto&& val) { return static_cast<int32_t>(val); });

    REQUIRE(result == 200);
}

// -----------------------------------------------------------------------------
// Same-size operations - now allowed (user responsibility)
// These do NOT warn under the new policy
// -----------------------------------------------------------------------------

TEST_CASE("Same-size: int accumulator for int elements", "[overflow][same-size]") {
    std::vector<int> data = {1, 2, 3, 4, 5};

    // No warning - same size is allowed, user knows their data
    auto result = ilp::reduce_range<4>(data, 0, std::plus<>{}, [](auto&& val) { return val; });

    REQUIRE(result == 15);
}

TEST_CASE("Same-size: int8_t accumulator for int8_t elements", "[overflow][same-size]") {
    std::vector<int8_t> data = {1, 2, 3, 4, 5};

    // No warning - same size is allowed
    // (overflow IS possible with more elements, but user is responsible)
    auto result = ilp::reduce_range<4>(data, int8_t{0}, std::plus<>{}, [](auto&& val) { return val; });

    REQUIRE(result == 15);
}

TEST_CASE("Same-size: uint32_t accumulator for uint32_t elements", "[overflow][same-size]") {
    std::vector<uint32_t> data = {1000000, 2000000, 3000000};

    // No warning - same size is allowed
    auto result = ilp::reduce_range<4>(data, uint32_t{0}, std::plus<>{}, [](auto&& val) { return val; });

    REQUIRE(result == 6000000);
}

// -----------------------------------------------------------------------------
// Unsafe operations - would produce deprecation warnings if not suppressed
// (accumulator strictly smaller than element/index type)
// -----------------------------------------------------------------------------

TEST_CASE("Unsafe: int16_t accumulator with int index", "[overflow][unsafe]") {
    // WARNING: accumulator (int16_t, 2 bytes) < index type (int, 4 bytes)
    auto result = ilp::reduce<4>(0, 10, int16_t{0}, std::plus<>{}, [](auto i) { return static_cast<int16_t>(i); });

    REQUIRE(result == 45);
}

TEST_CASE("Unsafe: int8_t accumulator with int16_t elements", "[overflow][unsafe]") {
    std::vector<int16_t> data = {1, 2, 3, 4, 5};

    // WARNING: accumulator (int8_t, 1 byte) < element type (int16_t, 2 bytes)
    auto result =
        ilp::reduce_range<4>(data, int8_t{0}, std::plus<>{}, [](auto&& val) { return static_cast<int8_t>(val); });

    REQUIRE(result == 15);
}

TEST_CASE("Unsafe: int8_t accumulator with int index", "[overflow][unsafe]") {
    // WARNING: accumulator (int8_t, 1 byte) < index type (int, 4 bytes)
    auto result = ilp::reduce<4>(0, 10, int8_t{0}, std::plus<>{}, [](auto i) { return static_cast<int8_t>(i); });

    REQUIRE(result == 45);
}

// -----------------------------------------------------------------------------
// Demonstration: Overflow still happens with same-size (no warning)
// -----------------------------------------------------------------------------

TEST_CASE("Demo: int8_t overflow with many elements (no warning)", "[overflow][demo]") {
    std::vector<int8_t> data(200, 1); // 200 elements, each = 1

    // No warning (same-size allowed), but WILL overflow at runtime!
    // int8_t can only hold -128 to 127
    auto result = ilp::reduce_range<4>(data, int8_t{0}, std::plus<>{}, [](auto&& val) { return val; });

    // Result is wrong due to overflow
    INFO("Result with overflow: " << static_cast<int>(result));
    // Don't REQUIRE correctness - this documents overflow behavior
}

// -----------------------------------------------------------------------------
// Workaround using explicit init type
// -----------------------------------------------------------------------------

TEST_CASE("Workaround: Explicit init with larger type", "[overflow][workaround]") {
    std::vector<int> data = {1, 2, 3, 4, 5};

    // Using reduce with explicit int64_t init
    auto result =
        ilp::reduce_range<4>(data, int64_t{0}, std::plus<>{}, [](auto&& val) { return static_cast<int64_t>(val); });

    REQUIRE(result == 15);
}

// -----------------------------------------------------------------------------
// Documentation
// -----------------------------------------------------------------------------
//
// Warning policy: sizeof(AccumT) < sizeof(ElemT)
//
// WARNS:
//   - Accumulator smaller than element type (e.g., int8_t accumulating int)
//
// DOES NOT WARN:
//   - Same-size accumulation (users know their data)
//   - Larger accumulator (safe)
//   - Floating point (different overflow semantics)
//
// To avoid overflow issues:
//   return static_cast<int64_t>(val);  // Use larger accumulator type
//   return static_cast<double>(val);   // Use floating point

#endif // !ILP_MODE_SIMPLE
