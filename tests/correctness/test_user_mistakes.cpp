#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <cstdint>
#include <limits>
#include <numeric>
#include <ranges>
#include <string>
#include <vector>

#if !defined(ILP_MODE_SIMPLE)

// =============================================================================
// TEST: Dumb Stuff Users Might Do
// =============================================================================

// -----------------------------------------------------------------------------
// Mistake 1: Inverted ranges (start > end)
// -----------------------------------------------------------------------------

TEST_CASE("Inverted range (start > end)", "[mistake][range]") {
    int count = 0;
    ILP_FOR(auto i, 10, 0, 4) { // Start > End!
        count++;
    }
    ILP_END;
    // Should this be 0 iterations or undefined behavior?
    REQUIRE(count == 0); // Expecting safe behavior
}

// -----------------------------------------------------------------------------
// Mistake 3: Using wrong loop type for the task
// -----------------------------------------------------------------------------

TEST_CASE("Using SIMPLE when need control flow", "[mistake][type]") {
    // User tries to "break" in a SIMPLE loop - this would be a compile error
    // but they might not understand why
    int sum = 0;
    ILP_FOR(auto i, 0, 100, 4) {
        if (i >= 10) {
            // Can't break here! But loop continues
        }
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 4950); // All iterations run
}

// -----------------------------------------------------------------------------
// Mistake 5: Empty body (why would they do this?)
// -----------------------------------------------------------------------------

TEST_CASE("Empty loop body", "[mistake][empty]") {
    ILP_FOR(auto i, 0, 100, 4) {
        // User forgot to do anything
        (void)i;
    }
    ILP_END;
    // At least it shouldn't crash
}

// -----------------------------------------------------------------------------
// Mistake 6: Modifying loop variable (would be confusing)
// -----------------------------------------------------------------------------

TEST_CASE("User tries to modify loop variable", "[mistake][modify]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 10, 4) {
        auto local_i = i; // They can't actually modify i
        local_i *= 2;     // This doesn't affect iteration
        sum += local_i;
    }
    ILP_END;
    REQUIRE(sum == 90); // 0+2+4+6+8+10+12+14+16+18
}

// -----------------------------------------------------------------------------
// Mistake 7: Using captures incorrectly
// -----------------------------------------------------------------------------

TEST_CASE("Capturing by value vs reference", "[mistake][capture]") {
    int value = 10;
    int result = 0;

    // The macro captures by [&], so this works
    ILP_FOR(auto i, 0, 5, 4) {
        result += value; // Uses reference
    }
    ILP_END;

    REQUIRE(result == 50);
}

// -----------------------------------------------------------------------------
// Mistake 8: Huge N value (ridiculous unrolling)
// -----------------------------------------------------------------------------

TEST_CASE("Absurdly large N=128", "[mistake][huge]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 10, 128) {
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 45);
}

// -----------------------------------------------------------------------------
// Mistake 9: Using wrong data types that might overflow
// -----------------------------------------------------------------------------

TEST_CASE("int8_t iteration", "[mistake][overflow]") {
    // Iterating with int8_t type
    int count = 0;
    ILP_FOR(auto i, (int8_t)0, (int8_t)100, 4) {
        count++;
        (void)i;
    }
    ILP_END;
    REQUIRE(count == 100);
}

TEST_CASE("Unsigned underflow danger", "[mistake][underflow]") {
    // User might not realize unsigned can't go negative
    unsigned sum = 0;
    ILP_FOR(auto i, 0u, 10u, 4) {
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 45);
}

// -----------------------------------------------------------------------------
// Mistake 10: Not understanding remainder handling
// -----------------------------------------------------------------------------

TEST_CASE("User confused about remainder", "[mistake][remainder]") {
    // N=4, range 0-9: main loop 0-7, remainder 8
    std::vector<int> values;
    values.reserve(9);
    ILP_FOR(auto i, 0, 9, 4) {
        values.push_back(i);
    }
    ILP_END;

    // User might expect different behavior
    REQUIRE(values.size() == 9);
    REQUIRE(values.back() == 8);
}

// -----------------------------------------------------------------------------
// Mistake 14: Using the same variable name as loop var
// -----------------------------------------------------------------------------

TEST_CASE("Shadow outer variable with loop var", "[mistake][shadow]") {
    int i = 999;
    int sum = 0;

    ILP_FOR(auto i, 0, 10, 4) { // Shadows outer i
        sum += i;
    }
    ILP_END;

    REQUIRE(sum == 45);
    REQUIRE(i == 999); // Outer i unchanged
}

// -----------------------------------------------------------------------------
// Mistake 15: Recursive/nested loop confusion
// -----------------------------------------------------------------------------

TEST_CASE("Nested loops with same variable name", "[mistake][nested]") {
    int count = 0;

    ILP_FOR(auto i, 0, 3, 4) {
        ILP_FOR(auto i, 0, 3, 4) { // Same name shadows outer
            count++;
        }
        ILP_END;
    }
    ILP_END;

    REQUIRE(count == 9);
}

// -----------------------------------------------------------------------------
// Mistake 19: Mixed signed/unsigned comparison issues
// -----------------------------------------------------------------------------

TEST_CASE("Mixed signed types", "[mistake][signed]") {
    // This might cause warnings or unexpected behavior
    int sum = 0;
    ILP_FOR(auto i, 0, 10, 4) {
        sum += static_cast<int>(i);
    }
    ILP_END;
    REQUIRE(sum == 45);
}

// -----------------------------------------------------------------------------
// Mistake 20: Using complex objects in hot loop
// -----------------------------------------------------------------------------

TEST_CASE("String concatenation in loop", "[mistake][performance]") {
    std::string result;
    ILP_FOR(auto i, 0, 5, 4) {
        result += std::to_string(i);
    }
    ILP_END;
    REQUIRE(result == "01234");
}

// -----------------------------------------------------------------------------
// Mistake 21: Allocating in loop
// -----------------------------------------------------------------------------

TEST_CASE("Allocating vectors in loop", "[mistake][performance]") {
    std::vector<std::vector<int>> all;
    all.reserve(5);
    ILP_FOR(auto i, 0, 5, 4) {
        all.push_back(std::vector<int>{i});
    }
    ILP_END;
    REQUIRE(all.size() == 5);
}

// -----------------------------------------------------------------------------
// Mistake 22: Using initializer list as range
// -----------------------------------------------------------------------------

TEST_CASE("Initializer list as range", "[mistake][init]") {
    int sum = 0;
    std::vector<int> temp = {1, 2, 3, 4, 5};
    ILP_FOR_RANGE(auto&& val, temp, 4) {
        sum += val;
    }
    ILP_END;
    REQUIRE(sum == 15);
}

// -----------------------------------------------------------------------------
// Mistake 24: Expecting C-style for semantics
// -----------------------------------------------------------------------------

TEST_CASE("Off-by-one expectations", "[mistake][semantics]") {
    // C-style: for(i=0; i<=10; i++) iterates 11 times
    // ILP: (0, 10) iterates 10 times (exclusive end)

    int count = 0;
    ILP_FOR(auto i, 0, 10, 4) {
        count++;
    }
    ILP_END;
    REQUIRE(count == 10); // NOT 11
}

// -----------------------------------------------------------------------------
// Mistake 25: Expecting to modify container while iterating
// -----------------------------------------------------------------------------

TEST_CASE("Reading during iteration (safe)", "[mistake][modify]") {
    std::vector<int> data = {1, 2, 3, 4, 5};
    int sum = 0;

    ILP_FOR_RANGE(auto&& val, data, 4) {
        sum += val;
        // User might want to modify data here - unsafe!
    }
    ILP_END;

    REQUIRE(sum == 15);
}

// -----------------------------------------------------------------------------
// Mistake 26: Using pointers instead of ranges
// -----------------------------------------------------------------------------

TEST_CASE("Array pointer iteration", "[mistake][pointer]") {
    int arr[] = {1, 2, 3, 4, 5};
    int sum = 0;

    // User needs to use index-based loop for raw arrays
    ILP_FOR(auto i, 0, 5, 4) {
        sum += arr[i];
    }
    ILP_END;

    REQUIRE(sum == 15);
}

// -----------------------------------------------------------------------------
// Mistake 33: Using temporary in range
// -----------------------------------------------------------------------------

TEST_CASE("Temporary vector in range", "[mistake][temporary]") {
    // This is actually fine because macro captures reference
    int sum = 0;
    std::vector<int> v = {1, 2, 3, 4, 5};
    ILP_FOR_RANGE(auto&& val, v, 4) {
        sum += val;
    }
    ILP_END;
    REQUIRE(sum == 15);
}

// -----------------------------------------------------------------------------
// Mistake 35: Const-correctness issues
// -----------------------------------------------------------------------------

TEST_CASE("Const data iteration", "[mistake][const]") {
    const std::array<int, 5> data = {1, 2, 3, 4, 5};
    int sum = 0;

    ILP_FOR_RANGE(auto&& val, data, 4) {
        sum += val;
        // val is const& here
    }
    ILP_END;

    REQUIRE(sum == 15);
}

// -----------------------------------------------------------------------------
// Mistake 36: Expecting index in range loop
// -----------------------------------------------------------------------------

TEST_CASE("Need index but using range loop", "[mistake][index]") {
    std::vector<int> data = {10, 20, 30, 40, 50};
    int sum_with_index = 0;

    // Wrong way - can't easily get index
    int idx = 0;
    ILP_FOR_RANGE(auto&& val, data, 4) {
        sum_with_index += val * idx;
        idx++;
    }
    ILP_END;

    REQUIRE(sum_with_index == 400); // 0*10 + 1*20 + 2*30 + 3*40 + 4*50
}

// -----------------------------------------------------------------------------
// Mistake 37: Misunderstanding return vs break
// -----------------------------------------------------------------------------

TEST_CASE("Return vs break confusion", "[mistake][control]") {
    // ILP_BREAK exits loop, ILP_RETURN exits function
    int sum = 0;

    ILP_FOR(auto i, 0, 100, 4) {
        if (i >= 10)
            ILP_BREAK; // Exit loop only
        sum += i;
    }
    ILP_END;

    REQUIRE(sum == 45);
}

#endif // !ILP_MODE_SIMPLE
