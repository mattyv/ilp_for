// Suppress deprecation warnings for this entire test file
// (intentionally tests large unroll factors which trigger warnings)
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <array>
#include <limits>
#include <cstdint>
#include <string>

// =============================================================================
// STRESS TEST: Cruel and Unusual Edge Cases
// =============================================================================

// -----------------------------------------------------------------------------
// Section 1: Unroll Factor Edge Cases
// -----------------------------------------------------------------------------

TEST_CASE("N=1 trivial unroll", "[edge][unroll]") {
    int sum = 0;
    ILP_FOR(void, auto i, 0, 10, 1) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 45);
}

TEST_CASE("N=2 minimal unroll", "[edge][unroll]") {
    int sum = 0;
    ILP_FOR(void, auto i, 0, 10, 2) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 45);
}

TEST_CASE("Large N > range size", "[edge][unroll]") {
    // N=16 but only 5 elements
    int sum = 0;
    ILP_FOR(void, auto i, 0, 5, 16) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 10);
}

TEST_CASE("Very large unroll factor", "[edge][unroll]") {
    // N=64 - does this even compile?
    int sum = 0;
    ILP_FOR(void, auto i, 0, 10, 64) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 45);
}

TEST_CASE("Prime unroll factors", "[edge][unroll]") {
    SECTION("N=3") {
        int sum = 0;
        ILP_FOR(void, auto i, 0, 10, 3) {
            sum += i;
        } ILP_END;
        REQUIRE(sum == 45);
    }

    SECTION("N=5") {
        int sum = 0;
        ILP_FOR(void, auto i, 0, 10, 5) {
            sum += i;
        } ILP_END;
        REQUIRE(sum == 45);
    }

    SECTION("N=7") {
        int sum = 0;
        ILP_FOR(void, auto i, 0, 10, 7) {
            sum += i;
        } ILP_END;
        REQUIRE(sum == 45);
    }

    SECTION("N=11") {
        int sum = 0;
        ILP_FOR(void, auto i, 0, 10, 11) {
            sum += i;
        } ILP_END;
        REQUIRE(sum == 45);
    }
}

// -----------------------------------------------------------------------------
// Section 2: Empty and Single Element Ranges
// -----------------------------------------------------------------------------

TEST_CASE("Empty range (start == end)", "[edge][empty]") {
    int count = 0;
    ILP_FOR(void, auto i, 0, 0, 4) {
        count++;
    } ILP_END;
    REQUIRE(count == 0);
}

TEST_CASE("Single element range", "[edge][single]") {
    int sum = 0;
    ILP_FOR(void, auto i, 0, 1, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 0);
}

TEST_CASE("Empty vector range", "[edge][empty]") {
    std::vector<int> empty;
    int count = 0;
    ILP_FOR_RANGE(void, auto&& val, empty, 4) {
        count++;
        (void)val;
    } ILP_END;
    REQUIRE(count == 0);
}

TEST_CASE("Single element vector", "[edge][single]") {
    std::vector<int> single = {42};
    int sum = 0;
    ILP_FOR_RANGE(void, auto&& val, single, 4) {
        sum += val;
    } ILP_END;
    REQUIRE(sum == 42);
}

// -----------------------------------------------------------------------------
// Section 3: Reduce Edge Cases
// -----------------------------------------------------------------------------

TEST_CASE("Reduce empty range", "[edge][reduce]") {
    auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 0, 4) {
        return i;
    } ILP_END_REDUCE;
    REQUIRE(result == 0);
}

TEST_CASE("Reduce single element", "[edge][reduce]") {
    auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 1, 4) {
        return i;
    } ILP_END_REDUCE;
    REQUIRE(result == 0);
}

TEST_CASE("Reduce with custom operation - min", "[edge][reduce]") {
    std::vector<int> data = {5, 3, 8, 1, 9, 2};
    auto result = ILP_REDUCE_RANGE(
        [](int a, int b) { return std::min(a, b); },
        std::numeric_limits<int>::max(),
        auto&& val, data, 4
    ) {
        return val;
    } ILP_END_REDUCE;
    REQUIRE(result == 1);
}

TEST_CASE("Reduce empty vector", "[edge][reduce]") {
    std::vector<int> empty;
    auto result = ILP_REDUCE_RANGE(std::plus<>{}, 0, auto&& val, empty, 4) {
        return val;
    } ILP_END_REDUCE;
    REQUIRE(result == 0);
}

// -----------------------------------------------------------------------------
// Section 4: Signed Integer Edge Cases
// -----------------------------------------------------------------------------

TEST_CASE("Negative indices", "[edge][signed]") {
    int sum = 0;
    ILP_FOR(void, auto i, -10, 0, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == -55);
}

TEST_CASE("Large negative to positive range", "[edge][signed]") {
    int sum = 0;
    ILP_FOR(void, auto i, -50, 50, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == -50);  // -50 + -49 + ... + 49 = -50
}

TEST_CASE("All negative range", "[edge][signed]") {
    int sum = 0;
    ILP_FOR(void, auto i, -20, -10, 4) {
        sum += i;
    } ILP_END;
    // -20 + -19 + ... + -11 = -155
    REQUIRE(sum == -155);
}

// -----------------------------------------------------------------------------
// Section 6: Type Edge Cases
// -----------------------------------------------------------------------------

TEST_CASE("int8_t small integers", "[edge][types]") {
    int8_t sum = 0;
    ILP_FOR(void, auto i, (int8_t)0, (int8_t)10, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 45);
}

TEST_CASE("uint64_t large integers", "[edge][types]") {
    uint64_t sum = 0;
    ILP_FOR(void, auto i, (uint64_t)0, (uint64_t)100, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 4950);
}

TEST_CASE("size_t iteration", "[edge][types]") {
    size_t sum = 0;
    ILP_FOR(void, auto i, (size_t)0, (size_t)10, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 45);
}

TEST_CASE("ptrdiff_t signed iteration", "[edge][types]") {
    ptrdiff_t sum = 0;
    ILP_FOR(void, auto i, (ptrdiff_t)-5, (ptrdiff_t)5, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == -5);
}

TEST_CASE("Different element types in vector", "[edge][types]") {
    SECTION("double") {
        std::vector<double> data = {1.5, 2.5, 3.5, 4.5};
        double sum = 0.0;
        ILP_FOR_RANGE(void, auto&& val, data, 4) {
            sum += val;
        } ILP_END;
        REQUIRE(sum == 12.0);
    }

    SECTION("float") {
        std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f};
        float sum = 0.0f;
        ILP_FOR_RANGE(void, auto&& val, data, 4) {
            sum += val;
        } ILP_END;
        REQUIRE(sum == 10.0f);
    }
}

// -----------------------------------------------------------------------------
// Section 7: Control Flow Edge Cases
// -----------------------------------------------------------------------------

#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)

TEST_CASE("Break on first iteration", "[edge][control]") {
    int count = 0;
    ILP_FOR(void, auto i, 0, 100, 4) {
        count++;
        if (i == 0) ILP_BREAK;
    } ILP_END;
    REQUIRE(count == 1);
}

TEST_CASE("Break on last iteration", "[edge][control]") {
    int count = 0;
    ILP_FOR(void, auto i, 0, 10, 4) {
        count++;
        if (i == 9) ILP_BREAK;
    } ILP_END;
    REQUIRE(count == 10);
}

TEST_CASE("Continue all iterations (no-op)", "[edge][control]") {
    int sum = 0;
    ILP_FOR(void, auto i, 0, 10, 4) {
        sum += i;
        ILP_CONTINUE;
    } ILP_END;
    REQUIRE(sum == 45);
}

TEST_CASE("Break in remainder iterations", "[edge][control]") {
    // N=4, range 0-10, break at 6 (in remainder)
    int sum = 0;
    ILP_FOR(void, auto i, 0, 10, 4) {
        if (i >= 6) ILP_BREAK;
        sum += i;
    } ILP_END;
    REQUIRE(sum == 15);  // 0+1+2+3+4+5
}

TEST_CASE("Alternating continue pattern", "[edge][control]") {
    int sum = 0;
    ILP_FOR(void, auto i, 0, 10, 4) {
        if (i % 2 == 0) ILP_CONTINUE;
        sum += i;
    } ILP_END;
    REQUIRE(sum == 25);  // 1+3+5+7+9
}

#endif

// -----------------------------------------------------------------------------
// Section 8: For-Until Edge Cases
// -----------------------------------------------------------------------------

TEST_CASE("Find returns sentinel when nothing found", "[edge][find]") {
    auto result = ILP_FIND(auto i, 0, 100, 4) {
        return i == 999;  // Never true
    } ILP_END;
    REQUIRE(result == 100);  // Sentinel (end)
}

TEST_CASE("Find first element", "[edge][find]") {
    auto result = ILP_FIND(auto i, 0, 100, 4) {
        return i == 0;
    } ILP_END;
    REQUIRE(result != 100);
    REQUIRE(result == 0);
}

TEST_CASE("Find last element", "[edge][find]") {
    auto result = ILP_FIND(auto i, 0, 100, 4) {
        return i == 99;
    } ILP_END;
    REQUIRE(result != 100);
    REQUIRE(result == 99);
}

TEST_CASE("Find in remainder", "[edge][find]") {
    // N=4, range 0-10, find 9 (in remainder)
    auto result = ILP_FIND(auto i, 0, 10, 4) {
        return i == 9;
    } ILP_END;
    REQUIRE(result != 10);
    REQUIRE(result == 9);
}

TEST_CASE("Find in empty range", "[edge][find]") {
    auto result = ILP_FIND(auto i, 0, 0, 4) {
        return true;  // Would find immediately, but empty
    } ILP_END;
    REQUIRE(result == 0);  // Sentinel (end of empty range)
}

TEST_CASE("Find range idx - empty vector", "[edge][find]") {
    std::vector<int> empty;
    auto result = ILP_FIND_RANGE_IDX(auto&& val, auto idx, empty, 4) {
        (void)val;
        (void)idx;
        return empty.end();  // Can't find
    } ILP_END;
    REQUIRE(result == empty.end());
}

// -----------------------------------------------------------------------------
// Section 9: Return Value Edge Cases
// -----------------------------------------------------------------------------

TEST_CASE("For-ret-simple finds value", "[edge][ret]") {
    auto result = ILP_FIND(auto i, 0, 100, 4) {
        if (i == 42) return i;
        return _ilp_end_;
    } ILP_END;
    REQUIRE(result == 42);
}

TEST_CASE("For-ret-simple finds nothing", "[edge][ret]") {
    auto result = ILP_FIND(auto i, 0, 100, 4) {
        if (i == 999) return i;
        return _ilp_end_;
    } ILP_END;
    REQUIRE(result == 100);  // Sentinel value
}

TEST_CASE("For-ret-simple empty range", "[edge][ret]") {
    auto result = ILP_FIND(auto i, 0, 0, 4) {
        return i;  // Would return immediately, but empty
    } ILP_END;
    REQUIRE(result == 0);  // Returns end (sentinel)
}

// -----------------------------------------------------------------------------
// Section 10: Range with Index Edge Cases
// -----------------------------------------------------------------------------

TEST_CASE("Range-idx finds index", "[edge][rangeidx]") {
    std::vector<int> data = {10, 20, 30, 40, 50};
    auto it = ILP_FIND_RANGE_IDX(auto&& val, auto idx, data, 4) {
        if (val == 30) return std::ranges::begin(data) + idx;
        return _ilp_end_;
    } ILP_END;
    REQUIRE(it != data.end());
    REQUIRE(*it == 30);
}

TEST_CASE("Range-idx empty vector", "[edge][rangeidx]") {
    std::vector<int> empty;
    auto it = ILP_FIND_RANGE_IDX(auto&& val, auto idx, empty, 4) {
        (void)val; (void)idx;
        return std::ranges::begin(empty);
    } ILP_END;
    REQUIRE(it == empty.end());
}

// -----------------------------------------------------------------------------
// Section 11: Exactly Divisible by N
// -----------------------------------------------------------------------------

TEST_CASE("Range exactly divisible by N", "[edge][divisible]") {
    SECTION("N=4, range=8") {
        int sum = 0;
        ILP_FOR(void, auto i, 0, 8, 4) {
            sum += i;
        } ILP_END;
        REQUIRE(sum == 28);
    }

    SECTION("N=4, range=16") {
        int sum = 0;
        ILP_FOR(void, auto i, 0, 16, 4) {
            sum += i;
        } ILP_END;
        REQUIRE(sum == 120);
    }

    SECTION("N=8, range=24") {
        int sum = 0;
        ILP_FOR(void, auto i, 0, 24, 8) {
            sum += i;
        } ILP_END;
        REQUIRE(sum == 276);
    }
}

// -----------------------------------------------------------------------------
// Section 12: Just Below/Above N Multiples
// -----------------------------------------------------------------------------

TEST_CASE("Range just below N multiple", "[edge][boundary]") {
    // N=4, range=7 (one below 8)
    int sum = 0;
    ILP_FOR(void, auto i, 0, 7, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 21);
}

TEST_CASE("Range just above N multiple", "[edge][boundary]") {
    // N=4, range=9 (one above 8)
    int sum = 0;
    ILP_FOR(void, auto i, 0, 9, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 36);
}

// -----------------------------------------------------------------------------
// Section 13: std::array Tests
// -----------------------------------------------------------------------------

TEST_CASE("std::array range", "[edge][array]") {
    std::array<int, 5> arr = {1, 2, 3, 4, 5};
    int sum = 0;
    ILP_FOR_RANGE(void, auto&& val, arr, 4) {
        sum += val;
    } ILP_END;
    REQUIRE(sum == 15);
}

TEST_CASE("Empty std::array", "[edge][array]") {
    std::array<int, 0> arr = {};
    int count = 0;
    ILP_FOR_RANGE(void, auto&& val, arr, 4) {
        count++;
        (void)val;
    } ILP_END;
    REQUIRE(count == 0);
}

// -----------------------------------------------------------------------------
// Section 14: Accumulator Modification Tests
// -----------------------------------------------------------------------------

TEST_CASE("Multiple accumulators", "[edge][accumulator]") {
    int sum = 0;
    int product = 1;
    ILP_FOR(void, auto i, 1, 6, 4) {
        sum += i;
        product *= i;
    } ILP_END;
    REQUIRE(sum == 15);
    REQUIRE(product == 120);
}

TEST_CASE("Conditional accumulation", "[edge][accumulator]") {
    int even_sum = 0;
    int odd_sum = 0;
    ILP_FOR(void, auto i, 0, 10, 4) {
        if (i % 2 == 0) even_sum += i;
        else odd_sum += i;
    } ILP_END;
    REQUIRE(even_sum == 20);  // 0+2+4+6+8
    REQUIRE(odd_sum == 25);   // 1+3+5+7+9
}

// -----------------------------------------------------------------------------
// Section 15: Nested Data Structure Access
// -----------------------------------------------------------------------------

TEST_CASE("Vector of vectors - inner sum", "[edge][nested]") {
    std::vector<std::vector<int>> data = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };
    int total = 0;
    ILP_FOR_RANGE(void, auto&& row, data, 4) {
        ILP_FOR_RANGE(void, auto&& val, row, 4) {
            total += val;
        } ILP_END;
    } ILP_END;
    REQUIRE(total == 45);
}

// -----------------------------------------------------------------------------
// Section 16: Large Range Tests
// -----------------------------------------------------------------------------

TEST_CASE("Large range sum", "[edge][large]") {
    // Sum 0..9999
    auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 10000, 4) {
        return i;
    } ILP_END_REDUCE;
    REQUIRE(result == 49995000);
}

TEST_CASE("Large vector sum", "[edge][large]") {
    std::vector<int> data(1000);
    for (int i = 0; i < 1000; ++i) data[i] = i;

    auto result = ILP_REDUCE_RANGE(std::plus<>{}, 0, auto&& val, data, 4) {
        return val;
    } ILP_END_REDUCE;
    REQUIRE(result == 499500);
}

// -----------------------------------------------------------------------------
// Section 17: Non-Starting-At-Zero Ranges
// -----------------------------------------------------------------------------

TEST_CASE("Non-zero start index", "[edge][offset]") {
    int sum = 0;
    ILP_FOR(void, auto i, 100, 110, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 1045);  // 100+101+...+109
}

TEST_CASE("Large offset range", "[edge][offset]") {
    int sum = 0;
    ILP_FOR(void, auto i, 1000000, 1000010, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 10000045);
}

// -----------------------------------------------------------------------------
// Section 18: Reduce with Break
// -----------------------------------------------------------------------------

#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)

TEST_CASE("Reduce with early break", "[edge][reduce][control]") {
    auto result = ILP_REDUCE(std::plus<>(), 0, auto i, 0, 100, 4) {
        if (i >= 10) ILP_REDUCE_BREAK;
        ILP_REDUCE_BREAK_VALUE(i);
    } ILP_END_REDUCE;
    REQUIRE(result == 45);  // 0+1+...+9
}

TEST_CASE("Reduce breaks on first", "[edge][reduce][control]") {
    auto result = ILP_REDUCE(std::plus<>(), 100, auto i, 0, 100, 4) {
        (void)i;
        ILP_REDUCE_BREAK;
    } ILP_END_REDUCE;
    REQUIRE(result == 100);  // Initial value
}

#endif

// -----------------------------------------------------------------------------
// Section 19: Modifying External State
// -----------------------------------------------------------------------------

TEST_CASE("Counter modification", "[edge][state]") {
    int counter = 0;
    ILP_FOR(void, auto i, 0, 10, 4) {
        (void)i;
        counter++;
    } ILP_END;
    REQUIRE(counter == 10);
}

TEST_CASE("Vector push_back in loop", "[edge][state]") {
    std::vector<int> collected;
    collected.reserve(10);
    ILP_FOR(void, auto i, 0, 10, 4) {
        collected.push_back(i);
    } ILP_END;
    REQUIRE(collected.size() == 10);
    REQUIRE(collected[0] == 0);
    REQUIRE(collected[9] == 9);
}

// -----------------------------------------------------------------------------
// Section 20: Mixed Operations
// -----------------------------------------------------------------------------

TEST_CASE("Sum with modulo filter", "[edge][mixed]") {
    auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 100, 4) {
        if (i % 3 == 0) return i;
        return 0;
    } ILP_END_REDUCE;
    // Sum of multiples of 3 from 0-99
    REQUIRE(result == 1683);
}

TEST_CASE("Product of range", "[edge][mixed]") {
    auto result = ILP_REDUCE(
        std::multiplies<>(), 1, auto i, 1, 6, 4
    ) {
        return i;
    } ILP_END_REDUCE;
    REQUIRE(result == 120);  // 5!
}

// -----------------------------------------------------------------------------
// Section 21: Exact N boundary with different starting points
// -----------------------------------------------------------------------------

TEST_CASE("Exactly N elements from non-zero start", "[edge][boundary]") {
    // N=4, exactly 4 elements
    int sum = 0;
    ILP_FOR(void, auto i, 10, 14, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 46);  // 10+11+12+13
}

TEST_CASE("N-1 elements", "[edge][boundary]") {
    // N=4, exactly 3 elements
    int sum = 0;
    ILP_FOR(void, auto i, 10, 13, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 33);  // 10+11+12
}

TEST_CASE("N+1 elements", "[edge][boundary]") {
    // N=4, exactly 5 elements
    int sum = 0;
    ILP_FOR(void, auto i, 10, 15, 4) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 60);  // 10+11+12+13+14
}

// -----------------------------------------------------------------------------
// Section 22: Auto-selecting variants
// -----------------------------------------------------------------------------

TEST_CASE("Auto-select reduce sum", "[edge][auto]") {
    auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 100, 4) {
        return i;
    } ILP_END_REDUCE;
    REQUIRE(result == 4950);
}

TEST_CASE("Auto-select range sum", "[edge][auto]") {
    std::vector<int> data = {1, 2, 3, 4, 5};
    auto result = ILP_REDUCE_RANGE(std::plus<>{}, 0, auto&& val, data, 4) {
        return val;
    } ILP_END_REDUCE;
    REQUIRE(result == 15);
}

// -----------------------------------------------------------------------------
// Section 23: Potentially Problematic Patterns
// -----------------------------------------------------------------------------

TEST_CASE("Reading uninitialized-looking memory", "[edge][memory]") {
    // Pre-allocated but not written
    std::vector<int> data(10, 0);
    int sum = 0;
    ILP_FOR_RANGE(void, auto&& val, data, 4) {
        sum += val;
    } ILP_END;
    REQUIRE(sum == 0);
}

TEST_CASE("Const range iteration", "[edge][const]") {
    const std::vector<int> data = {1, 2, 3, 4, 5};
    int sum = 0;
    ILP_FOR_RANGE(void, auto&& val, data, 4) {
        sum += val;
    } ILP_END;
    REQUIRE(sum == 15);
}

// -----------------------------------------------------------------------------
// Section 24: Edge cases with combinations
// -----------------------------------------------------------------------------

TEST_CASE("Empty range with large N", "[edge][combo]") {
    int count = 0;
    ILP_FOR(void, auto i, 0, 0, 64) {
        count++;
    } ILP_END;
    REQUIRE(count == 0);
}

TEST_CASE("Single element with large N", "[edge][combo]") {
    int sum = 0;
    ILP_FOR(void, auto i, 0, 1, 64) {
        sum += i;
    } ILP_END;
    REQUIRE(sum == 0);
}

// -----------------------------------------------------------------------------
// Section 25: Potential Integer Overflow Scenarios
// -----------------------------------------------------------------------------

TEST_CASE("Sum near max int", "[edge][overflow]") {
    // This tests accumulator overflow handling
    std::vector<int> data = {
        std::numeric_limits<int>::max() / 4,
        std::numeric_limits<int>::max() / 4,
        std::numeric_limits<int>::max() / 4,
        std::numeric_limits<int>::max() / 4
    };

    // Just verify it doesn't crash - overflow behavior is defined by int
    auto result = ILP_REDUCE_RANGE(std::plus<>{}, 0, auto&& val, data, 4) {
        return val;
    } ILP_END_REDUCE;
    (void)result;  // Just checking it runs
}

// -----------------------------------------------------------------------------
// Section 26: Verifying iteration order
// -----------------------------------------------------------------------------

TEST_CASE("Iteration order preserved", "[edge][order]") {
    std::vector<int> order;
    order.reserve(10);
    ILP_FOR(void, auto i, 0, 10, 4) {
        order.push_back(i);
    } ILP_END;

    for (int i = 0; i < 10; ++i) {
        REQUIRE(order[i] == i);
    }
}

TEST_CASE("Range iteration order preserved", "[edge][order]") {
    std::vector<int> data = {10, 20, 30, 40, 50};
    std::vector<int> order;
    order.reserve(5);
    ILP_FOR_RANGE(void, auto&& val, data, 4) {
        order.push_back(val);
    } ILP_END;

    REQUIRE(order == data);
}

// -----------------------------------------------------------------------------
// Section 28: Complex expression in loop body
// -----------------------------------------------------------------------------

TEST_CASE("Complex arithmetic in body", "[edge][complex]") {
    auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 1, 11, 4) {
        return (i * i - i + 1) * 2 - i;
    } ILP_END_REDUCE;

    int expected = 0;
    for (int i = 1; i < 11; ++i) {
        expected += (i * i - i + 1) * 2 - i;
    }
    REQUIRE(result == expected);
}

// -----------------------------------------------------------------------------
// Section 29: Testing all N values from 1 to 8
// -----------------------------------------------------------------------------

TEST_CASE("All N values 1-8", "[edge][comprehensive]") {
    auto expected = [](int n) {
        int sum = 0;
        for (int i = 0; i < n; ++i) sum += i;
        return sum;
    };

    int sum1 = 0; ILP_FOR(void, auto i, 0, 20, 1) { sum1 += i; } ILP_END;
    int sum2 = 0; ILP_FOR(void, auto i, 0, 20, 2) { sum2 += i; } ILP_END;
    int sum3 = 0; ILP_FOR(void, auto i, 0, 20, 3) { sum3 += i; } ILP_END;
    int sum4 = 0; ILP_FOR(void, auto i, 0, 20, 4) { sum4 += i; } ILP_END;
    int sum5 = 0; ILP_FOR(void, auto i, 0, 20, 5) { sum5 += i; } ILP_END;
    int sum6 = 0; ILP_FOR(void, auto i, 0, 20, 6) { sum6 += i; } ILP_END;
    int sum7 = 0; ILP_FOR(void, auto i, 0, 20, 7) { sum7 += i; } ILP_END;
    int sum8 = 0; ILP_FOR(void, auto i, 0, 20, 8) { sum8 += i; } ILP_END;

    REQUIRE(sum1 == expected(20));
    REQUIRE(sum2 == expected(20));
    REQUIRE(sum3 == expected(20));
    REQUIRE(sum4 == expected(20));
    REQUIRE(sum5 == expected(20));
    REQUIRE(sum6 == expected(20));
    REQUIRE(sum7 == expected(20));
    REQUIRE(sum8 == expected(20));
}
