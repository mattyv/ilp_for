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
// For-Until with Complex Predicates
// -----------------------------------------------------------------------------

TEST_CASE("For-until with stateful predicate", "[edge][until]") {
    std::vector<int> data(100);
    std::iota(data.begin(), data.end(), 0);

    int call_count = 0;
    auto it = ilp::find_if<4>(data, [&](auto val) {
        call_count++;
        return val == 50;
    });

    REQUIRE(it != data.end()); // Found
    REQUIRE(*it == 50);
    // call_count should be at least 51
    REQUIRE(call_count >= 51);
}

// -----------------------------------------------------------------------------
// Reduce with Non-Trivial Body
// -----------------------------------------------------------------------------

TEST_CASE("Reduce body with side effects", "[edge][reduce]") {
    int side_effect = 0;

    auto result = ilp::transform_reduce<4>(std::views::iota(0, 10), 0, std::plus<>{}, [&](auto i) {
        side_effect += i;
        return i;
    });

    REQUIRE(result == 45);
    REQUIRE(side_effect == 45); // Each i accessed once
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
// For-Ret with Exactly N Elements
// -----------------------------------------------------------------------------

TEST_CASE("For-ret-simple exact boundaries", "[edge][ret]") {
    std::vector<int> data = {0, 1, 2, 3};

    SECTION("Find at 0 with N elements") {
        auto it = ilp::find_if<4>(data, [](auto val) { return val == 0; });
        REQUIRE(it != data.end());
        REQUIRE(*it == 0);
    }

    SECTION("Find at N-1 with N elements") {
        auto it = ilp::find_if<4>(data, [](auto val) { return val == 3; });
        REQUIRE(it != data.end());
        REQUIRE(*it == 3);
    }

    SECTION("Find nothing with N elements") {
        auto it = ilp::find_if<4>(data, [](auto val) { return val == 99; });
        REQUIRE(it == data.end()); // Not found
    }
}

// -----------------------------------------------------------------------------
// Auto-Select with Different Element Sizes
// -----------------------------------------------------------------------------

TEST_CASE("Auto-select with int8_t", "[edge][auto]") {
    std::vector<int8_t> data = {1, 2, 3, 4, 5};

    auto result = ilp::transform_reduce<4>(data, 0, std::plus<>{}, [&](auto&& val) { return val; });

    REQUIRE(result == 15);
}

TEST_CASE("Auto-select with int64_t", "[edge][auto]") {
    std::vector<int64_t> data = {1, 2, 3, 4, 5};

    auto result = ilp::transform_reduce<4>(data, 0, std::plus<>{}, [&](auto&& val) { return val; });

    REQUIRE(result == 15);
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
// Reduce with Lambda Capture Issues
// -----------------------------------------------------------------------------

TEST_CASE("Reduce captures work correctly", "[edge][capture]") {
    int multiplier = 2;

    auto result = ilp::transform_reduce<4>(std::views::iota(0, 10), 0, std::plus<>{}, [&](auto i) { return i * multiplier; });

    // 0*2 + 1*2 + ... + 9*2 = 90
    REQUIRE(result == 90);
}

// -----------------------------------------------------------------------------
// Nested Range-Idx
// -----------------------------------------------------------------------------

TEST_CASE("Range-idx nested operations", "[edge][rangeidx]") {
    std::vector<int> data = {10, 20, 30, 40, 50};

    int sum = 0;
    int idx_sum = 0;

    auto it = ilp::find_range_idx<4>(data, [&](auto&& val, auto idx, auto _ilp_end_) {
        sum += val;
        idx_sum += idx;
        if (val == 30)
            return std::ranges::begin(data) + idx;
        return _ilp_end_;
    });

    REQUIRE(it != data.end());
    REQUIRE(*it == 30);
    // sum should be 10+20+30=60 (up to found element)
    REQUIRE(sum >= 60);
}

// -----------------------------------------------------------------------------
// Control Flow in Last Element
// -----------------------------------------------------------------------------

#if !defined(ILP_MODE_SIMPLE)

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

#endif

// -----------------------------------------------------------------------------
// For-Find Range with Index Tracking
// -----------------------------------------------------------------------------

TEST_CASE("Find range with index", "[edge][find]") {
    std::vector<int> data = {1, 2, 3, 4, 5};

    auto result = ilp::find_range_idx<4>(data, [&](auto&& val, auto idx, auto _ilp_end_) {
        if (val == 3)
            return std::ranges::begin(data) + idx;
        return _ilp_end_;
    });

    REQUIRE(result != data.end());
    REQUIRE(*result == 3);
}

// -----------------------------------------------------------------------------
// Double-Nested Reduce
// -----------------------------------------------------------------------------

TEST_CASE("Double-nested reduce", "[edge][nested][reduce]") {
    auto result = ilp::transform_reduce<4>(std::views::iota(0, 5), 0, std::plus<>{}, [&](auto i) {
        auto inner = ilp::transform_reduce<4>(std::views::iota(0, 5), 0, std::plus<>{}, [&](auto j) { return i + j; });
        return inner;
    });

    // For each i in [0,5): sum of (i+0)+(i+1)+(i+2)+(i+3)+(i+4) = 5i+10
    // Total: sum of (5i+10) for i in [0,5) = 5*(0+1+2+3+4) + 50 = 50+50 = 100
    REQUIRE(result == 100);
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

// -----------------------------------------------------------------------------
// Check that return types are correct
// -----------------------------------------------------------------------------

TEST_CASE("Return type preservation", "[edge][types]") {
    SECTION("Double return") {
        auto result = ilp::transform_reduce<4>(std::views::iota(0, 10), 0.0, std::plus<>(), [&](auto i) { return static_cast<double>(i); });
        REQUIRE(result == 45.0);
    }

    SECTION("Long long return") {
        auto result =
            ilp::transform_reduce<4>(std::views::iota(0, 10), (long long)0, std::plus<>(), [&](auto i) { return static_cast<long long>(i); });
        REQUIRE(result == 45LL);
    }
}

#endif // !ILP_MODE_SIMPLE
