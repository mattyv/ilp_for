#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <cstdint>
#include <limits>
#include <numeric>
#include <ranges>
#include <vector>

#if !defined(ILP_MODE_SIMPLE)

// =============================================================================
// EVIL TEST CASES: Really trying to break things
// =============================================================================

// -----------------------------------------------------------------------------
// Evil 1: Integer limits and overflow
// -----------------------------------------------------------------------------

TEST_CASE("Near max integer range", "[evil][limits]") {
    // Start near INT_MAX
    int64_t sum = 0;
    int start = std::numeric_limits<int>::max() - 10;
    int end = std::numeric_limits<int>::max() - 5;

    ILP_FOR(auto i, start, end, 4) {
        sum += i;
    }
    ILP_END;

    // Sum of 5 values starting from max-10
    int64_t expected = 0;
    for (int i = start; i < end; ++i)
        expected += i;
    REQUIRE(sum == expected);
}

TEST_CASE("Near min integer range", "[evil][limits]") {
    int64_t sum = 0;
    int start = std::numeric_limits<int>::min() + 5;
    int end = std::numeric_limits<int>::min() + 10;

    ILP_FOR(auto i, start, end, 4) {
        sum += i;
    }
    ILP_END;

    int64_t expected = 0;
    for (int i = start; i < end; ++i)
        expected += i;
    REQUIRE(sum == expected);
}

TEST_CASE("Size_t near max", "[evil][limits]") {
    size_t start = std::numeric_limits<size_t>::max() - 20;
    size_t end = std::numeric_limits<size_t>::max() - 10;

    int count = 0;
    ILP_FOR(auto i, start, end, 4) {
        count++;
        (void)i;
    }
    ILP_END;

    REQUIRE(count == 10);
}

// -----------------------------------------------------------------------------
// Evil 2: Inverted range behavior verification
// -----------------------------------------------------------------------------

TEST_CASE("Inverted unsigned range", "[evil][inverted]") {
    // This is undefined territory - what happens?
    unsigned count = 0;
    ILP_FOR(auto i, 10u, 0u, 4) { // Inverted!
        count++;
    }
    ILP_END;
    // Should be 0 iterations for safety
    REQUIRE(count == 0);
}

TEST_CASE("Inverted signed range", "[evil][inverted]") {
    int count = 0;
    ILP_FOR(auto i, 100, -100, 4) { // Inverted!
        count++;
    }
    ILP_END;
    REQUIRE(count == 0);
}

// -----------------------------------------------------------------------------
// Evil 3: N vs Range size mismatches
// -----------------------------------------------------------------------------

TEST_CASE("Exactly 2N elements", "[evil][boundary]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 8, 4) { // 8 = 2*4
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 28); // 0+1+2+3+4+5+6+7
}

TEST_CASE("Exactly 2N-1 elements", "[evil][boundary]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 7, 4) { // 7 = 2*4-1
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 21);
}

TEST_CASE("Exactly 2N+1 elements", "[evil][boundary]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 9, 4) { // 9 = 2*4+1
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 36);
}

// -----------------------------------------------------------------------------
// Evil 6: Control flow in every position
// -----------------------------------------------------------------------------

TEST_CASE("Break at N boundary", "[evil][control]") {
    // N=4, break exactly at position 4
    int sum = 0;
    ILP_FOR(auto i, 0, 100, 4) {
        if (i == 4)
            ILP_BREAK;
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 6); // 0+1+2+3
}

TEST_CASE("Break at N-1", "[evil][control]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 100, 4) {
        if (i == 3)
            ILP_BREAK;
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 3); // 0+1+2
}

TEST_CASE("Break at N+1", "[evil][control]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 100, 4) {
        if (i == 5)
            ILP_BREAK;
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 10); // 0+1+2+3+4
}

TEST_CASE("Break at 2N", "[evil][control]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 100, 4) {
        if (i == 8)
            ILP_BREAK;
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 28); // 0+1+...+7
}

// -----------------------------------------------------------------------------
// Evil 7: Weird type combinations
// -----------------------------------------------------------------------------

TEST_CASE("Mixing int and size_t", "[evil][types]") {
    size_t sum = 0;
    ILP_FOR(auto i, 0, 10, 4) {
        sum += static_cast<size_t>(i);
    }
    ILP_END;
    REQUIRE(sum == 45);
}

TEST_CASE("int16_t accumulator with int iteration", "[evil][types]") {
    int16_t sum = 0;
    ILP_FOR(auto i, 0, 100, 4) {
        sum += static_cast<int16_t>(i);
    }
    ILP_END;
    REQUIRE(sum == 4950);
}

// -----------------------------------------------------------------------------
// Evil 9: Vector edge cases
// -----------------------------------------------------------------------------

TEST_CASE("Vector with one element less than N", "[evil][vector]") {
    std::vector<int> data = {1, 2, 3}; // 3 < N=4
    int sum = 0;
    ILP_FOR_RANGE(auto&& val, data, 4) {
        sum += val;
    }
    ILP_END;
    REQUIRE(sum == 6);
}

TEST_CASE("Vector exactly N elements", "[evil][vector]") {
    std::vector<int> data = {1, 2, 3, 4}; // 4 = N
    int sum = 0;
    ILP_FOR_RANGE(auto&& val, data, 4) {
        sum += val;
    }
    ILP_END;
    REQUIRE(sum == 10);
}

// -----------------------------------------------------------------------------
// Evil 11: Iteration order stress test
// -----------------------------------------------------------------------------

TEST_CASE("Strict iteration order for side effects", "[evil][order]") {
    std::vector<int> order;
    order.reserve(20);

    ILP_FOR(auto i, 0, 20, 4) {
        order.push_back(i);
    }
    ILP_END;

    // MUST be strictly sequential
    for (int i = 0; i < 20; ++i) {
        REQUIRE(order[i] == i);
    }
}

TEST_CASE("Range iteration order verification", "[evil][order]") {
    std::vector<int> data(20);
    std::iota(data.begin(), data.end(), 0);

    std::vector<int> order;
    order.reserve(20);

    ILP_FOR_RANGE(auto&& val, data, 4) {
        order.push_back(val);
    }
    ILP_END;

    REQUIRE(order == data);
}

#endif // !ILP_MODE_SIMPLE
