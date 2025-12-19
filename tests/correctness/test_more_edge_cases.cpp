#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <cstdint>
#include <limits>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

#if !defined(ILP_MODE_SIMPLE)

// =============================================================================
// MORE EDGE CASES - Seeking additional issues
// =============================================================================

// -----------------------------------------------------------------------------
// Integer Overflow in Loop Calculations
// -----------------------------------------------------------------------------

TEST_CASE("Range size calculation overflow", "[edge][overflow]") {
    // If (end - start) overflows, bad things happen
    // This is a potential issue with very large ranges

    SECTION("Safe large range") {
        // Just verify we can handle reasonably large numbers
        int64_t sum = 0;
        ILP_FOR(auto i, (int64_t)0, (int64_t)1000000, 4) {
            sum += 1;
        }
        ILP_END;
        REQUIRE(sum == 1000000);
    }
}

// -----------------------------------------------------------------------------
// Negative Index Iteration
// -----------------------------------------------------------------------------

TEST_CASE("Large negative ranges", "[edge][negative]") {
    int64_t sum = 0;
    ILP_FOR(auto i, -1000, -900, 4) {
        sum += i;
    }
    ILP_END;

    int64_t expected = 0;
    for (int i = -1000; i < -900; ++i)
        expected += i;
    REQUIRE(sum == expected);
}

// -----------------------------------------------------------------------------
// Mixed N and Range Sizes
// -----------------------------------------------------------------------------

TEST_CASE("Range exactly = N*k + r for various r", "[edge][remainder]") {
    auto sum = [](int n) {
        int s = 0;
        for (int i = 0; i < n; ++i)
            s += i;
        return s;
    };

    // N=4, test r=0,1,2,3
    for (int r = 0; r < 4; ++r) {
        int range_size = 16 + r; // 4*4 + r
        int s = 0;
        if (r == 0) {
            ILP_FOR(auto i, 0, 16, 4) {
                s += i;
            }
            ILP_END;
        } else if (r == 1) {
            ILP_FOR(auto i, 0, 17, 4) {
                s += i;
            }
            ILP_END;
        } else if (r == 2) {
            ILP_FOR(auto i, 0, 18, 4) {
                s += i;
            }
            ILP_END;
        } else {
            ILP_FOR(auto i, 0, 19, 4) {
                s += i;
            }
            ILP_END;
        }

        REQUIRE(s == sum(range_size));
    }
}

// -----------------------------------------------------------------------------
// Vector Subrange Iteration
// -----------------------------------------------------------------------------

TEST_CASE("std::span iteration", "[edge][span]") {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::span<int> sp(data.data() + 2, 5); // {3,4,5,6,7}

    int sum = 0;
    ILP_FOR_RANGE(auto&& val, sp, 4) {
        sum += val;
    }
    ILP_END;

    REQUIRE(sum == 25); // 3+4+5+6+7
}

// -----------------------------------------------------------------------------
// Range with Const Elements
// -----------------------------------------------------------------------------

TEST_CASE("Const element type", "[edge][const]") {
    const std::vector<int> data = {1, 2, 3, 4, 5};
    int sum = 0;
    ILP_FOR_RANGE(auto&& val, data, 4) {
        sum += val;
    }
    ILP_END;
    REQUIRE(sum == 15);
}

// -----------------------------------------------------------------------------
// Control Flow in Last Element
// -----------------------------------------------------------------------------

TEST_CASE("Break on exactly last element", "[edge][control]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 10, 4) {
        sum += i;
        if (i == 9)
            ILP_BREAK;
    }
    ILP_END;

    REQUIRE(sum == 45); // All elements processed
}

TEST_CASE("Continue on last element", "[edge][control]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 10, 4) {
        if (i == 9)
            ILP_CONTINUE;
        sum += i;
    }
    ILP_END;

    REQUIRE(sum == 36); // 45 - 9
}

// -----------------------------------------------------------------------------
// Zero-Sized Types (Unusual)
// -----------------------------------------------------------------------------

TEST_CASE("Empty struct in vector", "[edge][empty]") {
    struct Empty {};
    std::vector<Empty> data(10);

    int count = 0;
    ILP_FOR_RANGE(auto&& val, data, 4) {
        count++;
        (void)val;
    }
    ILP_END;

    REQUIRE(count == 10);
}

// -----------------------------------------------------------------------------
// Various Odd N Values
// -----------------------------------------------------------------------------

TEST_CASE("Odd N values - N=3", "[edge][oddN]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 10, 3) {
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 45);
}

TEST_CASE("Odd N values - N=5", "[edge][oddN]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 10, 5) {
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 45);
}

TEST_CASE("Odd N values - N=7", "[edge][oddN]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 10, 7) {
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 45);
}

#endif // !ILP_MODE_SIMPLE
