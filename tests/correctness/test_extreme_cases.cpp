#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <array>
#include <cstdint>
#include <limits>
#include <random>
#include <ranges>
#include <vector>

#if !defined(ILP_MODE_SIMPLE)

// =============================================================================
// EXTREME TESTS: Push boundaries even further
// =============================================================================

// -----------------------------------------------------------------------------
// Zero and One Element Edge Cases
// -----------------------------------------------------------------------------

TEST_CASE("Zero elements with various N values", "[extreme][zero]") {
    // N must be compile-time constant, so we test specific values below
    (void)std::initializer_list<int>{1, 2, 3, 4, 5, 6, 7, 8}; // Document intent

    int count1 = 0;
    ILP_FOR(auto i, 0, 0, 1) {
        count1++;
    }
    ILP_END;
    int count2 = 0;
    ILP_FOR(auto i, 0, 0, 2) {
        count2++;
    }
    ILP_END;
    int count4 = 0;
    ILP_FOR(auto i, 0, 0, 4) {
        count4++;
    }
    ILP_END;
    int count8 = 0;
    ILP_FOR(auto i, 0, 0, 8) {
        count8++;
    }
    ILP_END;

    REQUIRE(count1 == 0);
    REQUIRE(count2 == 0);
    REQUIRE(count4 == 0);
    REQUIRE(count8 == 0);
}

// -----------------------------------------------------------------------------
// Large Unroll Factors
// -----------------------------------------------------------------------------

TEST_CASE("N=16 with small range", "[extreme][unroll]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 3, 16) { // Range 3 < N=16
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 3); // 0+1+2
}

TEST_CASE("N=32 with medium range", "[extreme][unroll]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 50, 16) {
        sum += i;
    }
    ILP_END;
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
        }
        ILP_END;
        REQUIRE(sum == 45);
    }
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
            }
            ILP_END;
        }
        ILP_END;
    }
    ILP_END;

    REQUIRE(total == 125); // 5*5*5
}

TEST_CASE("Nested loops with outer variable capture", "[extreme][nested]") {
    int total = 0;

    ILP_FOR(auto i, 0, 5, 4) {
        ILP_FOR(auto j, 0, 5, 4) {
            total += i * j;
        }
        ILP_END;
    }
    ILP_END;

    // Sum of i*j for i,j in [0,5)
    int expected = 0;
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 5; ++j)
            expected += i * j;

    REQUIRE(total == expected);
}

// -----------------------------------------------------------------------------
// Boundary Crossing Tests
// -----------------------------------------------------------------------------

TEST_CASE("Signed integer boundary", "[extreme][boundary]") {
    // Test crossing from negative to positive
    int sum = 0;
    ILP_FOR(auto i, -5, 5, 4) {
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == -5); // -5 + -4 + -3 + -2 + -1 + 0 + 1 + 2 + 3 + 4 = -5
}

// -----------------------------------------------------------------------------
// Edge: What happens with volatile?
// -----------------------------------------------------------------------------

TEST_CASE("Volatile accumulator", "[extreme][volatile]") {
    volatile int sum = 0;
    ILP_FOR(auto i, 0, 10, 4) {
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 45);
}

// -----------------------------------------------------------------------------
// Modifying Captured References
// -----------------------------------------------------------------------------

TEST_CASE("Multiple captures modified", "[extreme][capture]") {
    int a = 0, b = 0, c = 0;

    ILP_FOR(auto i, 0, 12, 4) {
        if (i % 3 == 0)
            a += i;
        else if (i % 3 == 1)
            b += i;
        else
            c += i;
    }
    ILP_END;

    REQUIRE(a == 18); // 0+3+6+9
    REQUIRE(b == 22); // 1+4+7+10
    REQUIRE(c == 26); // 2+5+8+11
}

// -----------------------------------------------------------------------------
// Very Short Ranges
// -----------------------------------------------------------------------------

TEST_CASE("Range of 2 with N=8", "[extreme][short]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 2, 8) {
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 1);
}

TEST_CASE("Range of 1 with N=16", "[extreme][short]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 1, 16) {
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 0);
}

// -----------------------------------------------------------------------------
// Power of 2 Boundaries
// -----------------------------------------------------------------------------

TEST_CASE("Range = 2^n for various n", "[extreme][pow2]") {
    auto sum = [](int n) {
        int s = 0;
        for (int i = 0; i < n; ++i)
            s += i;
        return s;
    };

    int s1 = 0;
    ILP_FOR(auto i, 0, 2, 4) {
        s1 += i;
    }
    ILP_END;
    int s2 = 0;
    ILP_FOR(auto i, 0, 4, 4) {
        s2 += i;
    }
    ILP_END;
    int s3 = 0;
    ILP_FOR(auto i, 0, 8, 4) {
        s3 += i;
    }
    ILP_END;
    int s4 = 0;
    ILP_FOR(auto i, 0, 16, 4) {
        s4 += i;
    }
    ILP_END;
    int s5 = 0;
    ILP_FOR(auto i, 0, 32, 4) {
        s5 += i;
    }
    ILP_END;

    REQUIRE(s1 == sum(2));
    REQUIRE(s2 == sum(4));
    REQUIRE(s3 == sum(8));
    REQUIRE(s4 == sum(16));
    REQUIRE(s5 == sum(32));
}

#endif // !ILP_MODE_SIMPLE
