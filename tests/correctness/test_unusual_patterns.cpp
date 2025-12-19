#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <bit>
#include <exception>
#include <limits>
#include <ranges>
#include <vector>

#if !defined(ILP_MODE_SIMPLE)

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
            if (i == 50)
                throw std::runtime_error("test");
        }
        ILP_END;
        REQUIRE(false); // Should not reach
    } catch (const std::runtime_error& e) {
        // Exception caught
        REQUIRE(count >= 51); // At least up to 50
    }
}

// -----------------------------------------------------------------------------
// Zero-sized iteration variables
// -----------------------------------------------------------------------------

TEST_CASE("ptrdiff_t boundary", "[unusual][types]") {
    ptrdiff_t sum = 0;
    ILP_FOR(auto i, (ptrdiff_t)-5, (ptrdiff_t)5, 4) {
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == -5);
}

// -----------------------------------------------------------------------------
// Modifying different indices in array
// -----------------------------------------------------------------------------

TEST_CASE("Parallel array modification", "[unusual][array]") {
    std::array<int, 10> arr = {};

    ILP_FOR(auto i, 0, 10, 4) {
        arr[i] = i * i;
    }
    ILP_END;

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
    }
    ILP_END;

    REQUIRE(sum == 550); // Sum of all data
}

// -----------------------------------------------------------------------------
// Complex conditional accumulation
// -----------------------------------------------------------------------------

TEST_CASE("FizzBuzz-style conditional", "[unusual][conditional]") {
    int fizz = 0, buzz = 0, fizzbuzz = 0, other = 0;

    ILP_FOR(auto i, 1, 101, 4) {
        if (i % 15 == 0)
            fizzbuzz++;
        else if (i % 3 == 0)
            fizz++;
        else if (i % 5 == 0)
            buzz++;
        else
            other++;
    }
    ILP_END;

    REQUIRE(fizzbuzz == 6); // 15,30,45,60,75,90
    REQUIRE(fizz == 27);    // 33 - 6
    REQUIRE(buzz == 14);    // 20 - 6
    REQUIRE(other == 53);   // 100 - 47
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
    }
    ILP_END;

    REQUIRE(*max_ptr == 9);
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
    }
    ILP_END;

    REQUIRE(sum == 45);
    REQUIRE(count == 10);
}

// -----------------------------------------------------------------------------
// String building
// -----------------------------------------------------------------------------

TEST_CASE("String concatenation order", "[unusual][string]") {
    std::string result;
    result.reserve(10);

    ILP_FOR(auto i, 0, 5, 4) {
        result += static_cast<char>('a' + i);
    }
    ILP_END;

    REQUIRE(result == "abcde");
}

// -----------------------------------------------------------------------------
// Nested data access
// -----------------------------------------------------------------------------

TEST_CASE("Struct field access", "[unusual][struct]") {
    struct Point {
        int x, y;
    };
    std::vector<Point> points = {{1, 2}, {3, 4}, {5, 6}, {7, 8}};

    int x_sum = 0, y_sum = 0;
    ILP_FOR_RANGE(auto&& p, points, 4) {
        x_sum += p.x;
        y_sum += p.y;
    }
    ILP_END;

    REQUIRE(x_sum == 16); // 1+3+5+7
    REQUIRE(y_sum == 20); // 2+4+6+8
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
    }
    ILP_END;

    REQUIRE(counter == 1000);
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
        if (val < 5)
            less_than_5++;
        else if (val == 5)
            equal_to_5++;
        else
            greater_than_5++;
    }
    ILP_END;

    REQUIRE(less_than_5 == 3);    // 3, 1, 2
    REQUIRE(equal_to_5 == 1);     // 5
    REQUIRE(greater_than_5 == 3); // 8, 9, 7
}

#endif // !ILP_MODE_SIMPLE
