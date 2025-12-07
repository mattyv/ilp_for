#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <limits>
#include <exception>
#include <bit>

#if !defined(ILP_MODE_SUPER_SIMPLE)

// =============================================================================
// UNUSUAL PATTERNS - Testing weird/rare use cases
// =============================================================================

// -----------------------------------------------------------------------------
// Throwing in loop body
// -----------------------------------------------------------------------------

TEST_CASE("Exception thrown in loop body", "[unusual][exception]") {
    int count = 0;

    try {
        ILP_FOR(auto i, 0, 100, 4) {
            count++;
            if (i == 50) throw std::runtime_error("test");
        } ILP_END;
        REQUIRE(false);  // Should not reach
    } catch (const std::runtime_error& e) {
        // Exception caught
        REQUIRE(count >= 51);  // At least up to 50
    }
}

TEST_CASE("Exception in reduce body", "[unusual][exception]") {
    try {
        auto result = ilp::reduce<4>(0, 100, 0, std::plus<>{}, [&](auto i) {
            if (i == 50) throw std::runtime_error("test");
            return i;
        });
        (void)result;
        REQUIRE(false);
    } catch (const std::runtime_error&) {
        // OK
    }
}

// -----------------------------------------------------------------------------
// Zero-sized iteration variables
// -----------------------------------------------------------------------------

TEST_CASE("ptrdiff_t boundary", "[unusual][types]") {
    ptrdiff_t sum = 0;
    ILP_FOR(auto i, (ptrdiff_t)-5, (ptrdiff_t)5, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == -5);
}

// -----------------------------------------------------------------------------
// Boolean operations
// -----------------------------------------------------------------------------

TEST_CASE("Any_of pattern", "[unusual][pattern]") {
    std::vector<int> data = {1, 3, 5, 7, 8, 9};  // 8 is even

    auto result = ilp::find_range_idx<4>(data, [&](auto&& val, auto idx, auto end) {
        if (val % 2 == 0) return std::ranges::begin(data) + idx;
        return end;
    });

    REQUIRE(result != data.end());
    REQUIRE(result - data.begin() == 4);  // Index 4
}

TEST_CASE("All_of pattern (inverted)", "[unusual][pattern]") {
    std::vector<int> data = {2, 4, 6, 8, 10};

    auto result = ilp::find_range_idx<4>(data, [&](auto&& val, auto idx, auto end) {
        if (val % 2 != 0) return std::ranges::begin(data) + idx;  // Find first non-even
        return end;
    });

    REQUIRE(result == data.end());  // All even
}

// -----------------------------------------------------------------------------
// Reduce with identity results
// -----------------------------------------------------------------------------

TEST_CASE("Reduce always returns zero", "[unusual][reduce]") {
    auto result = ilp::reduce<4>(0, 100, 0, std::plus<>{}, [&](auto) {
        return 0;  // Always zero
    });
    REQUIRE(result == 0);
}

TEST_CASE("Reduce always returns same value", "[unusual][reduce]") {
    auto result = ilp::reduce<4>(0, 100, 0, std::plus<>{}, [&](auto i) {
        (void)i;
        return 42;
    });
    REQUIRE(result == 4200);  // 42 * 100
}

// -----------------------------------------------------------------------------
// Modifying different indices in array
// -----------------------------------------------------------------------------

TEST_CASE("Parallel array modification", "[unusual][array]") {
    std::array<int, 10> arr = {};

    ILP_FOR(auto i, 0, 10, 4) {
        arr[i] = i * i;
    } ILP_END;

    for (int i = 0; i < 10; ++i) {
        REQUIRE(arr[i] == i * i);
    }
}

// -----------------------------------------------------------------------------
// Indirect indexing
// -----------------------------------------------------------------------------

TEST_CASE("Indirect array access", "[unusual][indirect]") {
    std::vector<int> indices = {5, 2, 8, 1, 9, 0, 7, 3, 6, 4};
    std::vector<int> data = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};

    int sum = 0;
    ILP_FOR_RANGE(auto&& idx, indices, 4) {
        sum += data[idx];
    } ILP_END;

    REQUIRE(sum == 550);  // Sum of all data
}

// -----------------------------------------------------------------------------
// Complex conditional accumulation
// -----------------------------------------------------------------------------

TEST_CASE("FizzBuzz-style conditional", "[unusual][conditional]") {
    int fizz = 0, buzz = 0, fizzbuzz = 0, other = 0;

    ILP_FOR(auto i, 1, 101, 4) {
        if (i % 15 == 0) fizzbuzz++;
        else if (i % 3 == 0) fizz++;
        else if (i % 5 == 0) buzz++;
        else other++;
    } ILP_END;

    REQUIRE(fizzbuzz == 6);   // 15,30,45,60,75,90
    REQUIRE(fizz == 27);      // 33 - 6
    REQUIRE(buzz == 14);      // 20 - 6
    REQUIRE(other == 53);     // 100 - 47
}

// -----------------------------------------------------------------------------
// Accumulating pointers
// -----------------------------------------------------------------------------

TEST_CASE("Pointer arithmetic", "[unusual][pointer]") {
    int arr[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    int* max_ptr = &arr[0];
    ILP_FOR(auto i, 0, 10, 4) {
        if (arr[i] > *max_ptr) {
            max_ptr = &arr[i];
        }
    } ILP_END;

    REQUIRE(*max_ptr == 9);
}

// -----------------------------------------------------------------------------
// Floating point edge cases
// -----------------------------------------------------------------------------

TEST_CASE("Float NaN propagation", "[unusual][float]") {
    std::vector<double> data = {1.0, 2.0, std::nan(""), 4.0};

    auto result = ilp::reduce_range<4>(data, 0.0, std::plus<>(), [&](auto&& val) {
        return val;
    });

    REQUIRE(std::isnan(result));
}

TEST_CASE("Float infinity", "[unusual][float]") {
    std::vector<double> data = {1.0, 2.0, std::numeric_limits<double>::infinity()};

    auto result = ilp::reduce_range<4>(data, 0.0, std::plus<>(), [&](auto&& val) {
        return val;
    });

    REQUIRE(std::isinf(result));
}

// -----------------------------------------------------------------------------
// Multi-return value simulation
// -----------------------------------------------------------------------------

TEST_CASE("Returning multiple values", "[unusual][multiret]") {
    int sum = 0;
    int count = 0;

    ILP_FOR(auto i, 0, 10, 4) {
        sum += i;
        count++;
    } ILP_END;

    REQUIRE(sum == 45);
    REQUIRE(count == 10);
}

// -----------------------------------------------------------------------------
// Bit manipulation
// -----------------------------------------------------------------------------

TEST_CASE("Bit counting", "[unusual][bits]") {
    auto popcount = ilp::reduce<4>(0, 256, 0, std::plus<>{}, [&](auto i) {
        return std::popcount(static_cast<unsigned>(i));
    });

    // Sum of popcount for 0-255
    // Each bit position 0-7 is set in exactly 128 numbers
    REQUIRE(popcount == 1024);  // 8 * 128
}

// -----------------------------------------------------------------------------
// String building
// -----------------------------------------------------------------------------

TEST_CASE("String concatenation order", "[unusual][string]") {
    std::string result;
    result.reserve(10);

    ILP_FOR(auto i, 0, 5, 4) {
        result += static_cast<char>('a' + i);
    } ILP_END;

    REQUIRE(result == "abcde");
}

// -----------------------------------------------------------------------------
// Nested data access
// -----------------------------------------------------------------------------

TEST_CASE("Struct field access", "[unusual][struct]") {
    struct Point { int x, y; };
    std::vector<Point> points = {{1,2}, {3,4}, {5,6}, {7,8}};

    int x_sum = 0, y_sum = 0;
    ILP_FOR_RANGE(auto&& p, points, 4) {
        x_sum += p.x;
        y_sum += p.y;
    } ILP_END;

    REQUIRE(x_sum == 16);  // 1+3+5+7
    REQUIRE(y_sum == 20);  // 2+4+6+8
}

// -----------------------------------------------------------------------------
// Early termination patterns
// -----------------------------------------------------------------------------

TEST_CASE("Find first duplicate", "[unusual][pattern]") {
    std::vector<int> data = {1, 2, 3, 2, 4, 5};
    std::vector<bool> seen(10, false);

    auto result = ilp::find_range_idx<4>(data, [&](auto&& val, auto idx, auto end) {
        if (seen[val]) return std::ranges::begin(data) + idx;
        seen[val] = true;
        return end;
    });

    REQUIRE(result != data.end());
    REQUIRE(result - data.begin() == 3);  // Index 3 is first duplicate (value 2)
}

// -----------------------------------------------------------------------------
// Verify no partial writes
// -----------------------------------------------------------------------------

TEST_CASE("Atomic-like increments", "[unusual][atomic]") {
    // Without actual atomics, but verifying no torn writes
    int counter = 0;

    ILP_FOR(auto i, 0, 1000, 4) {
        (void)i;
        counter++;
    } ILP_END;

    REQUIRE(counter == 1000);
}

// -----------------------------------------------------------------------------
// Lambda with large capture
// -----------------------------------------------------------------------------

TEST_CASE("Large capture set", "[unusual][capture]") {
    int a = 1, b = 2, c = 3, d = 4, e = 5;
    int f = 6, g = 7, h = 8, i_outer = 9, j = 10;

    auto result = ilp::reduce<4>(0, 5, 0, std::plus<>{}, [&](auto i) {
        return a + b + c + d + e + f + g + h + i_outer + j + i;
    });

    // (1+2+3+4+5+6+7+8+9+10) = 55 for each, plus 0+1+2+3+4 = 10
    REQUIRE(result == 55 * 5 + 10);
}

// -----------------------------------------------------------------------------
// Different comparison operations
// -----------------------------------------------------------------------------

TEST_CASE("Count comparisons", "[unusual][compare]") {
    std::vector<int> data = {5, 3, 8, 1, 9, 2, 7};

    int less_than_5 = 0;
    int equal_to_5 = 0;
    int greater_than_5 = 0;

    ILP_FOR_RANGE(auto&& val, data, 4) {
        if (val < 5) less_than_5++;
        else if (val == 5) equal_to_5++;
        else greater_than_5++;
    } ILP_END;

    REQUIRE(less_than_5 == 3);   // 3, 1, 2
    REQUIRE(equal_to_5 == 1);    // 5
    REQUIRE(greater_than_5 == 3); // 8, 9, 7
}

// -----------------------------------------------------------------------------
// Reduce to non-numeric type
// -----------------------------------------------------------------------------

TEST_CASE("Reduce to pair", "[unusual][nonnum]") {
    using Pair = std::pair<int, int>;

    auto op = [](Pair a, Pair b) {
        return Pair{std::min(a.first, b.first),
                   std::max(a.second, b.second)};
    };
    auto init = Pair{std::numeric_limits<int>::max(),
                     std::numeric_limits<int>::min()};

    auto result = ilp::reduce<4>(0, 100, init, op, [&](auto i) {
        return Pair{i, i};  // Both min and max candidate is i
    });

    REQUIRE(result.first == 0);   // Min
    REQUIRE(result.second == 99); // Max
}
#endif // !ILP_MODE_SUPER_SIMPLE
