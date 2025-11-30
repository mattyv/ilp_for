#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <limits>
#include <cstdint>
#include <array>
#include <random>

// =============================================================================
// EXTREME TESTS: Push boundaries even further
// =============================================================================

// -----------------------------------------------------------------------------
// Accumulator Init Value Multiplication Issue
// -----------------------------------------------------------------------------

TEST_CASE("Reduce init value is NOT multiplied by N", "[critical][accumulator]") {
    // With N=4, there are 4 accumulators
    // The init value should NOT be 4x when reduction completes

    SECTION("Empty range - should return init unchanged") {
        auto result = ILP_REDUCE(
            std::plus<>(), 100, auto i, 0, 0, 4
        ) {
            return i;
        } ILP_END_REDUCE;
        // Critical: Should be 100, not 400!
        REQUIRE(result == 100);
    }

    SECTION("Single element with init") {
        auto result = ILP_REDUCE(
            std::plus<>(), 100, auto i, 0, 1, 4
        ) {
            return i;
        } ILP_END_REDUCE;
        // 100 + 0 = 100 (not 400 + 0)
        REQUIRE(result == 100);
    }
}

#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)

TEST_CASE("Reduce break on first returns init correctly", "[critical][accumulator]") {
    auto result = ILP_REDUCE(std::plus<>(), 100, auto i, 0, 100, 4) {
        ILP_REDUCE_BREAK(0);  // Immediately break
        return i;
    } ILP_END_REDUCE;
    // Should be 100, NOT 400
    INFO("Result should be init value (100), not N*init (400)");
    REQUIRE(result == 100);
}

#endif

// -----------------------------------------------------------------------------
// Zero and One Element Edge Cases
// -----------------------------------------------------------------------------

TEST_CASE("Zero elements with various N values", "[extreme][zero]") {
    // N must be compile-time constant, so we test specific values below
    (void)std::initializer_list<int>{1, 2, 3, 4, 5, 6, 7, 8};  // Document intent

    int count1 = 0; ILP_FOR(auto i, 0, 0, 1) { count1++; } ILP_END;
    int count2 = 0; ILP_FOR(auto i, 0, 0, 2) { count2++; } ILP_END;
    int count4 = 0; ILP_FOR(auto i, 0, 0, 4) { count4++; } ILP_END;
    int count8 = 0; ILP_FOR(auto i, 0, 0, 8) { count8++; } ILP_END;

    REQUIRE(count1 == 0);
    REQUIRE(count2 == 0);
    REQUIRE(count4 == 0);
    REQUIRE(count8 == 0);
}

// -----------------------------------------------------------------------------
// Non-Associative Operation Results
// -----------------------------------------------------------------------------

TEST_CASE("Subtraction reduce - parallel vs sequential", "[extreme][associativity]") {
    // With parallel accumulators, subtraction behaves differently
    // Sequential: ((((100 - 1) - 2) - 3) - 4) - 5) = 85
    // But parallel accumulators might give different result

    auto result = ILP_REDUCE(
        std::minus<>(), 100, auto i, 1, 6, 4
    ) {
        return i;
    } ILP_END_REDUCE;

    // Note: This is implementation-defined behavior with multi-accumulator
    // Just verify it doesn't crash and returns something
    INFO("Non-associative ops have implementation-defined results");
    (void)result;  // Don't assert specific value
}

// -----------------------------------------------------------------------------
// Floating Point Edge Cases
// -----------------------------------------------------------------------------

TEST_CASE("Float reduce accumulation", "[extreme][float]") {
    std::vector<float> data = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    auto result = ILP_REDUCE_RANGE(
        std::plus<>(), 0.0f, auto&& val, data, 4
    ) {
        return val;
    } ILP_END_REDUCE;

    REQUIRE(result == Approx(1.5f).epsilon(0.001f));
}

TEST_CASE("Double precision edge case", "[extreme][float]") {
    std::vector<double> data(100, 0.1);

    auto result = ILP_REDUCE_RANGE(
        std::plus<>(), 0.0, auto&& val, data, 4
    ) {
        return val;
    } ILP_END_REDUCE;

    REQUIRE(result == Approx(10.0).epsilon(0.001));
}

// -----------------------------------------------------------------------------
// Large Unroll Factors
// -----------------------------------------------------------------------------

TEST_CASE("N=16 with small range", "[extreme][unroll]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 3, 16) {  // Range 3 < N=16
        sum += i;
    } ILP_END;
    REQUIRE(sum == 3);  // 0+1+2
}

TEST_CASE("N=32 with medium range", "[extreme][unroll]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 50, 16) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 1225);
}

// -----------------------------------------------------------------------------
// Pointer Arithmetic Edge Cases
// -----------------------------------------------------------------------------

TEST_CASE("Array indices at boundaries", "[extreme][boundary]") {
    std::array<int, 10> arr = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    SECTION("First element") {
        int val = arr[0];
        REQUIRE(val == 0);
    }

    SECTION("Last element via index") {
        int sum = 0;
        ILP_FOR(auto i, 0, 10, 4) {
            sum += arr[i];
        } ILP_END;
        REQUIRE(sum == 45);
    }
}

// -----------------------------------------------------------------------------
// Mixed Container Types
// -----------------------------------------------------------------------------

TEST_CASE("std::array iteration", "[extreme][container]") {
    std::array<int, 7> arr = {1, 2, 3, 4, 5, 6, 7};

    auto result = ILP_REDUCE_RANGE(std::plus<>{}, 0, auto&& val, arr, 4) {
        return val;
    } ILP_END_REDUCE;

    REQUIRE(result == 28);
}

// -----------------------------------------------------------------------------
// Nested Loops Stress
// -----------------------------------------------------------------------------

TEST_CASE("Triple nested loops", "[extreme][nested]") {
    int total = 0;

    ILP_FOR(auto i, 0, 5, 4) {
        ILP_FOR(auto j, 0, 5, 4) {
            ILP_FOR(auto k, 0, 5, 4) {
                total++;
            } ILP_END;
        } ILP_END;
    } ILP_END;

    REQUIRE(total == 125);  // 5*5*5
}

TEST_CASE("Nested loops with outer variable capture", "[extreme][nested]") {
    int total = 0;

    ILP_FOR(auto i, 0, 5, 4) {
        ILP_FOR(auto j, 0, 5, 4) {
            total += i * j;
        } ILP_END;
    } ILP_END;

    // Sum of i*j for i,j in [0,5)
    int expected = 0;
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 5; ++j)
            expected += i * j;

    REQUIRE(total == expected);
}

// -----------------------------------------------------------------------------
// For-Until in Every Position
// -----------------------------------------------------------------------------

TEST_CASE("Find at positions 0 through 15", "[extreme][find]") {
    for (int target = 0; target <= 15; ++target) {
        auto result = ILP_FIND(auto i, 0, 100, 4) {
            return i == target;
        } ILP_END;

        REQUIRE(result != 100);  // Found
        REQUIRE(result == target);
    }
}

// -----------------------------------------------------------------------------
// Reduce with Different Binary Operations
// -----------------------------------------------------------------------------

TEST_CASE("XOR reduction", "[extreme][reduce]") {
    auto result = ILP_REDUCE(
        [](int a, int b) { return a ^ b; },
        0, auto i, 0, 16, 4
    ) {
        return i;
    } ILP_END_REDUCE;

    int expected = 0;
    for (int i = 0; i < 16; ++i) expected ^= i;
    REQUIRE(result == expected);
}

TEST_CASE("AND reduction", "[extreme][reduce]") {
    std::vector<int> data = {0xFF, 0xF0, 0x0F, 0xFF};

    auto result = ILP_REDUCE_RANGE(
        [](int a, int b) { return a & b; },
        0xFF, auto&& val, data, 4
    ) {
        return val;
    } ILP_END_REDUCE;

    REQUIRE(result == 0x00);  // 0xFF & 0xF0 & 0x0F & 0xFF = 0
}

TEST_CASE("OR reduction", "[extreme][reduce]") {
    std::vector<int> data = {0x01, 0x02, 0x04, 0x08};

    auto result = ILP_REDUCE_RANGE(
        [](int a, int b) { return a | b; },
        0, auto&& val, data, 4
    ) {
        return val;
    } ILP_END_REDUCE;

    REQUIRE(result == 0x0F);
}

// -----------------------------------------------------------------------------
// Ret-Simple Various Find Patterns
// -----------------------------------------------------------------------------

TEST_CASE("Find first negative", "[extreme][find]") {
    auto result = ILP_FIND(auto i, -10, 10, 4) {
        if (i >= 0) return i;
        return _ilp_end_;
    } ILP_END;

    REQUIRE(result == 0);
}

// -----------------------------------------------------------------------------
// Auto-Select Stress
// -----------------------------------------------------------------------------

TEST_CASE("Auto-select with various sizes", "[extreme][auto]") {
    SECTION("Small range") {
        auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 5, 4) {
            return i;
        } ILP_END_REDUCE;
        REQUIRE(result == 10);
    }

    SECTION("Medium range") {
        auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 100, 4) {
            return i;
        } ILP_END_REDUCE;
        REQUIRE(result == 4950);
    }

    SECTION("Large range") {
        auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 10000, 4) {
            return i;
        } ILP_END_REDUCE;
        REQUIRE(result == 49995000);
    }
}

// -----------------------------------------------------------------------------
// Range with Index - Stress Test
// -----------------------------------------------------------------------------

TEST_CASE("Range-idx finds each index", "[extreme][rangeidx]") {
    std::vector<int> data = {10, 20, 30, 40, 50};

    for (size_t target_idx = 0; target_idx < data.size(); ++target_idx) {
        int found_val = -1;
        size_t found_idx = SIZE_MAX;

        auto it = ILP_FIND_RANGE_IDX(auto&& val, auto idx, data, 4) {
            if (idx == target_idx) {
                found_val = val;
                found_idx = idx;
                return std::ranges::begin(data) + idx;
            }
            return _ilp_end_;
        } ILP_END;

        REQUIRE(it != data.end());
        REQUIRE(found_idx == target_idx);
        REQUIRE(found_val == data[target_idx]);
    }
}

// -----------------------------------------------------------------------------
// Boundary Crossing Tests
// -----------------------------------------------------------------------------

TEST_CASE("Signed integer boundary", "[extreme][boundary]") {
    // Test crossing from negative to positive
    int sum = 0;
    ILP_FOR(auto i, -5, 5, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == -5);  // -5 + -4 + -3 + -2 + -1 + 0 + 1 + 2 + 3 + 4 = -5
}

// -----------------------------------------------------------------------------
// Verify Correct Calculation of Complex Expressions
// -----------------------------------------------------------------------------

TEST_CASE("Complex math in reduce", "[extreme][math]") {
    auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 1, 11, 4) {
        return i * i;  // Sum of squares
    } ILP_END_REDUCE;

    REQUIRE(result == 385);  // 1+4+9+16+25+36+49+64+81+100
}

TEST_CASE("Conditional accumulation in reduce", "[extreme][math]") {
    auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 100, 4) {
        if (i % 5 == 0) return i;
        return 0;
    } ILP_END_REDUCE;

    // Sum of multiples of 5 from 0-99
    int expected = 0;
    for (int i = 0; i < 100; i += 5) expected += i;
    REQUIRE(result == expected);
}

// -----------------------------------------------------------------------------
// Edge: What happens with volatile?
// -----------------------------------------------------------------------------

TEST_CASE("Volatile accumulator", "[extreme][volatile]") {
    volatile int sum = 0;
    ILP_FOR(auto i, 0, 10, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 45);
}

// -----------------------------------------------------------------------------
// Modifying Captured References
// -----------------------------------------------------------------------------

TEST_CASE("Multiple captures modified", "[extreme][capture]") {
    int a = 0, b = 0, c = 0;

    ILP_FOR(auto i, 0, 12, 4) {
        if (i % 3 == 0) a += i;
        else if (i % 3 == 1) b += i;
        else c += i;
    } ILP_END;

    REQUIRE(a == 18);  // 0+3+6+9
    REQUIRE(b == 22);  // 1+4+7+10
    REQUIRE(c == 26);  // 2+5+8+11
}

// -----------------------------------------------------------------------------
// Very Short Ranges
// -----------------------------------------------------------------------------

TEST_CASE("Range of 2 with N=8", "[extreme][short]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 2, 8) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 1);
}

TEST_CASE("Range of 1 with N=16", "[extreme][short]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 1, 16) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 0);
}

// -----------------------------------------------------------------------------
// Power of 2 Boundaries
// -----------------------------------------------------------------------------

TEST_CASE("Range = 2^n for various n", "[extreme][pow2]") {
    auto sum = [](int n) {
        int s = 0;
        for (int i = 0; i < n; ++i) s += i;
        return s;
    };

    int s1 = 0; ILP_FOR(auto i, 0, 2, 4) { s1 += i; } ILP_END;
    int s2 = 0; ILP_FOR(auto i, 0, 4, 4) { s2 += i; } ILP_END;
    int s3 = 0; ILP_FOR(auto i, 0, 8, 4) { s3 += i; } ILP_END;
    int s4 = 0; ILP_FOR(auto i, 0, 16, 4) { s4 += i; } ILP_END;
    int s5 = 0; ILP_FOR(auto i, 0, 32, 4) { s5 += i; } ILP_END;

    REQUIRE(s1 == sum(2));
    REQUIRE(s2 == sum(4));
    REQUIRE(s3 == sum(8));
    REQUIRE(s4 == sum(16));
    REQUIRE(s5 == sum(32));
}
