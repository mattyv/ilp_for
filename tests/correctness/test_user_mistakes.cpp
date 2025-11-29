#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <limits>
#include <cstdint>
#include <string>

// =============================================================================
// TEST: Dumb Stuff Users Might Do
// =============================================================================

// -----------------------------------------------------------------------------
// Mistake 1: Inverted ranges (start > end)
// -----------------------------------------------------------------------------

TEST_CASE("Inverted range (start > end)", "[mistake][range]") {
    int count = 0;
    ILP_FOR(auto i, 10, 0, 4) {  // Start > End!
        count++;
    } ILP_END;
    // Should this be 0 iterations or undefined behavior?
    REQUIRE(count == 0);  // Expecting safe behavior
}

// -----------------------------------------------------------------------------
// Mistake 2: Wrong end variable usage in FOR_RET_SIMPLE
// -----------------------------------------------------------------------------

TEST_CASE("Return sentinel correctly", "[mistake][sentinel]") {
    // User might not understand _ilp_end_ usage
    auto result = ILP_FIND(auto i, 0, 100, 4) {
        if (i == 50) return i;
        return _ilp_end_;  // Correct usage
    } ILP_END;
    REQUIRE(result == 50);
}

// -----------------------------------------------------------------------------
// Mistake 3: Using wrong loop type for the task
// -----------------------------------------------------------------------------

#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)

TEST_CASE("Using SIMPLE when need control flow", "[mistake][type]") {
    // User tries to "break" in a SIMPLE loop - this would be a compile error
    // but they might not understand why
    int sum = 0;
    ILP_FOR(auto i, 0, 100, 4) {
        if (i >= 10) {
            // Can't break here! But loop continues
        }
        sum += i;
    } ILP_END;
    REQUIRE(sum == 4950);  // All iterations run
}

#endif

// -----------------------------------------------------------------------------
// Mistake 4: Forgetting to return in FOR_RET_SIMPLE
// -----------------------------------------------------------------------------

// This would be a compile error, but users might not understand why

// -----------------------------------------------------------------------------
// Mistake 5: Empty body (why would they do this?)
// -----------------------------------------------------------------------------

TEST_CASE("Empty loop body", "[mistake][empty]") {
    ILP_FOR(auto i, 0, 100, 4) {
        // User forgot to do anything
        (void)i;
    } ILP_END;
    // At least it shouldn't crash
}

TEST_CASE("Empty reduce body", "[mistake][empty]") {
    auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 100, 4) {
        (void)i;
        return 0;  // Always returns 0
    } ILP_END_REDUCE;
    REQUIRE(result == 0);
}

// -----------------------------------------------------------------------------
// Mistake 6: Modifying loop variable (would be confusing)
// -----------------------------------------------------------------------------

TEST_CASE("User tries to modify loop variable", "[mistake][modify]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 10, 4) {
        auto local_i = i;  // They can't actually modify i
        local_i *= 2;  // This doesn't affect iteration
        sum += local_i;
    } ILP_END;
    REQUIRE(sum == 90);  // 0+2+4+6+8+10+12+14+16+18
}

// -----------------------------------------------------------------------------
// Mistake 7: Using captures incorrectly
// -----------------------------------------------------------------------------

TEST_CASE("Capturing by value vs reference", "[mistake][capture]") {
    int value = 10;
    int result = 0;

    // The macro captures by [&], so this works
    ILP_FOR(auto i, 0, 5, 4) {
        result += value;  // Uses reference
    } ILP_END;

    REQUIRE(result == 50);
}

// -----------------------------------------------------------------------------
// Mistake 8: Huge N value (ridiculous unrolling)
// -----------------------------------------------------------------------------

TEST_CASE("Absurdly large N=128", "[mistake][huge]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 10, 128) {
        sum += i;
    } ILP_END;
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
    } ILP_END;
    REQUIRE(count == 100);
}

TEST_CASE("Unsigned underflow danger", "[mistake][underflow]") {
    // User might not realize unsigned can't go negative
    unsigned sum = 0;
    ILP_FOR(auto i, 0u, 10u, 4) {
        sum += i;
    } ILP_END;
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
    } ILP_END;

    // User might expect different behavior
    REQUIRE(values.size() == 9);
    REQUIRE(values.back() == 8);
}

// -----------------------------------------------------------------------------
// Mistake 11: Trying to use break in for_until
// -----------------------------------------------------------------------------

TEST_CASE("Using return false for continue in find", "[mistake][find]") {
    int count = 0;
    auto result = ILP_FIND(auto i, 0, 10, 4) {
        count++;
        return i == 5;  // Find 5
    } ILP_END;

    REQUIRE(result != 10);  // Found
    REQUIRE(result == 5);
    // Note: count may be > 6 due to unrolling semantics
}

// -----------------------------------------------------------------------------
// Mistake 12: Expecting reduce to preserve order for non-associative ops
// -----------------------------------------------------------------------------

TEST_CASE("Non-commutative reduction (subtraction)", "[mistake][associativity]") {
    // Subtraction is not associative!
    // ((((0 - 1) - 2) - 3) - 4) = -10
    // But with multiple accumulators, order changes

    auto result = ILP_REDUCE(
        std::minus<>(), 0, auto i, 1, 5, 4
    ) {
        return i;
    } ILP_END_REDUCE;

    // Note: This might give different results due to parallel accumulation!
    // Expected with sequential: 0-1-2-3-4 = -10
    (void)result;  // Just checking it doesn't crash
}

// -----------------------------------------------------------------------------
// Mistake 13: Zero-length vector with operations
// -----------------------------------------------------------------------------

TEST_CASE("Operations on empty vector", "[mistake][empty]") {
    std::vector<int> empty;

    // Min of empty set
    auto min_result = ILP_REDUCE_RANGE(
        [](int a, int b) { return std::min(a, b); },
        std::numeric_limits<int>::max(),
        auto&& val, empty, 4
    ) {
        return val;
    } ILP_END_REDUCE;
    REQUIRE(min_result == std::numeric_limits<int>::max());

    // Max of empty set
    auto max_result = ILP_REDUCE_RANGE(
        [](int a, int b) { return std::max(a, b); },
        std::numeric_limits<int>::min(),
        auto&& val, empty, 4
    ) {
        return val;
    } ILP_END_REDUCE;
    REQUIRE(max_result == std::numeric_limits<int>::min());
}

// -----------------------------------------------------------------------------
// Mistake 14: Using the same variable name as loop var
// -----------------------------------------------------------------------------

TEST_CASE("Shadow outer variable with loop var", "[mistake][shadow]") {
    int i = 999;
    int sum = 0;

    ILP_FOR(auto i, 0, 10, 4) {  // Shadows outer i
        sum += i;
    } ILP_END;

    REQUIRE(sum == 45);
    REQUIRE(i == 999);  // Outer i unchanged
}

// -----------------------------------------------------------------------------
// Mistake 15: Recursive/nested loop confusion
// -----------------------------------------------------------------------------

TEST_CASE("Nested loops with same variable name", "[mistake][nested]") {
    int count = 0;

    ILP_FOR(auto i, 0, 3, 4) {
        ILP_FOR(auto i, 0, 3, 4) {  // Same name shadows outer
            count++;
        } ILP_END;
    } ILP_END;

    REQUIRE(count == 9);
}

// -----------------------------------------------------------------------------
// Mistake 16: Expecting _ilp_ctrl in SIMPLE loops
// -----------------------------------------------------------------------------

// This would be a compile error - good!
// Users need to use ILP_FOR instead of ILP_FOR for control flow

// -----------------------------------------------------------------------------
// Mistake 17: Float/double as index type
// -----------------------------------------------------------------------------

// This should fail to compile due to std::integral constraint
// TEST_CASE("Float index", "[mistake][type]") {
//     ILP_FOR(i, 0.0, 10.0, 4) { } ILP_END;
// }

// -----------------------------------------------------------------------------
// Mistake 18: Very large range with overflow potential
// -----------------------------------------------------------------------------

TEST_CASE("Sum of large range", "[mistake][overflow]") {
    // Sum of 0..999999 = 499999500000 (needs 64-bit)
    uint64_t result = ILP_REDUCE(std::plus<>{}, 0, auto i, (uint64_t)0, (uint64_t)1000000, 4) {
        return i;
    } ILP_END_REDUCE;
    REQUIRE(result == 499999500000ULL);
}

// -----------------------------------------------------------------------------
// Mistake 19: Mixed signed/unsigned comparison issues
// -----------------------------------------------------------------------------

TEST_CASE("Mixed signed types", "[mistake][signed]") {
    // This might cause warnings or unexpected behavior
    int sum = 0;
    ILP_FOR(auto i, 0, 10, 4) {
        sum += static_cast<int>(i);
    } ILP_END;
    REQUIRE(sum == 45);
}

// -----------------------------------------------------------------------------
// Mistake 20: Using complex objects in hot loop
// -----------------------------------------------------------------------------

TEST_CASE("String concatenation in loop", "[mistake][performance]") {
    std::string result;
    ILP_FOR(auto i, 0, 5, 4) {
        result += std::to_string(i);
    } ILP_END;
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
    } ILP_END;
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
    } ILP_END;
    REQUIRE(sum == 15);
}

// -----------------------------------------------------------------------------
// Mistake 23: Boolean result confusion in for_until
// -----------------------------------------------------------------------------

TEST_CASE("Inverted boolean logic in find", "[mistake][logic]") {
    // User thinks true = continue, false = stop (inverted)
    int last = -1;
    auto result = ILP_FIND(auto i, 0, 10, 4) {
        last = i;
        return i >= 5;  // Stop when >= 5
    } ILP_END;

    REQUIRE(result != 10);  // Found
    REQUIRE(result >= 5);
    // last should be 5 or higher (unrolling may affect exact value)
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
    } ILP_END;
    REQUIRE(count == 10);  // NOT 11
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
    } ILP_END;

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
    } ILP_END;

    REQUIRE(sum == 15);
}

// -----------------------------------------------------------------------------
// Mistake 27: Trying to break from reduce_simple
// -----------------------------------------------------------------------------

// reduce_simple doesn't have _ilp_ctrl - would be compile error

// -----------------------------------------------------------------------------
// Mistake 29: Comparing iterators incorrectly
// -----------------------------------------------------------------------------

TEST_CASE("Iterator comparison in find_range_idx", "[mistake][iterator]") {
    std::vector<int> data = {1, 2, 3, 42, 5};

    auto it = ILP_FIND_RANGE_IDX(auto&& val, auto idx, data, 4) {
        if (val == 42) return std::ranges::begin(data) + idx;
        return _ilp_end_;
    } ILP_END;

    if (it != data.end()) {
        REQUIRE(*it == 42);
    }
}

// -----------------------------------------------------------------------------
// Mistake 30: Using wrong reduce operation
// -----------------------------------------------------------------------------

TEST_CASE("Using plus for product (wrong op)", "[mistake][operator]") {
    // User means to multiply but uses plus
    auto result = ILP_REDUCE(
        std::plus<>(), 0, auto i, 1, 5, 4  // Should be multiplies with init 1
    ) {
        return i;
    } ILP_END_REDUCE;
    REQUIRE(result == 10);  // Got sum instead of product
}

// -----------------------------------------------------------------------------
// Mistake 31: Division in reduce (non-associative)
// -----------------------------------------------------------------------------

TEST_CASE("Division reduce (problematic)", "[mistake][associativity]") {
    // Division is not associative!
    auto result = ILP_REDUCE(
        std::divides<>(), 1000, auto i, 1, 4, 4
    ) {
        return i;
    } ILP_END_REDUCE;
    // 1000 / 1 / 2 / 3 = 166 (integer division)
    // But parallel accumulators might give different result
    (void)result;  // Just check it runs
}

// -----------------------------------------------------------------------------
// Mistake 32: Expecting lazy evaluation
// -----------------------------------------------------------------------------

TEST_CASE("All iterations evaluate even if not needed", "[mistake][lazy]") {
    int eval_count = 0;

    auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 100, 4) {
        eval_count++;
        return i;
    } ILP_END_REDUCE;

    REQUIRE(result == 4950);
    REQUIRE(eval_count == 100);  // All evaluated
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
    } ILP_END;
    REQUIRE(sum == 15);
}

// -----------------------------------------------------------------------------
// Mistake 34: Huge vector iteration
// -----------------------------------------------------------------------------

TEST_CASE("Large vector performance", "[mistake][performance]") {
    std::vector<int> large(10000);
    for (int i = 0; i < 10000; ++i) large[i] = i;

    auto result = ILP_REDUCE_RANGE(std::plus<>{}, 0, auto&& val, large, 4) {
        return val;
    } ILP_END_REDUCE;

    REQUIRE(result == 49995000);
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
    } ILP_END;

    REQUIRE(sum == 15);
}

// -----------------------------------------------------------------------------
// Mistake 36: Expecting index in range loop
// -----------------------------------------------------------------------------

TEST_CASE("Need index but using range loop", "[mistake][index]") {
    std::vector<int> data = {10, 20, 30, 40, 50};
    int sum_with_index = 0;

    // Wrong way - can't easily get index
    // User should use ILP_FIND_RANGE_IDX instead
    int idx = 0;
    ILP_FOR_RANGE(auto&& val, data, 4) {
        sum_with_index += val * idx;
        idx++;
    } ILP_END;

    REQUIRE(sum_with_index == 400);  // 0*10 + 1*20 + 2*30 + 3*40 + 4*50
}

// -----------------------------------------------------------------------------
// Mistake 37: Misunderstanding return vs break
// -----------------------------------------------------------------------------

#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)

TEST_CASE("Return vs break confusion", "[mistake][control]") {
    // ILP_BREAK exits loop, ILP_RETURN exits function
    int sum = 0;

    ILP_FOR(auto i, 0, 100, 4) {
        if (i >= 10) ILP_BREAK;  // Exit loop only
        sum += i;
    } ILP_END;

    REQUIRE(sum == 45);
}

#endif

// -----------------------------------------------------------------------------
// Mistake 38: Using auto-select when manual is better
// -----------------------------------------------------------------------------

TEST_CASE("Auto-select vs manual N", "[mistake][auto]") {
    // Sometimes manual N is better for specific use case
    auto auto_result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 100, 4) {
        return i;
    } ILP_END_REDUCE;

    auto manual_result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 100, 8) {
        return i;
    } ILP_END_REDUCE;

    REQUIRE(auto_result == manual_result);
}

// -----------------------------------------------------------------------------
// Mistake 39: Expecting short-circuit in reduce
// -----------------------------------------------------------------------------

TEST_CASE("No short-circuit in reduce", "[mistake][shortcircuit]") {
    int count = 0;

    auto result = ILP_REDUCE(
        [](int a, int b) { return a + b; },
        0, auto i, 0, 100, 4
    ) {
        count++;
        return i;
    } ILP_END_REDUCE;

    REQUIRE(result == 4950);
    REQUIRE(count == 100);  // All evaluated
}

// -----------------------------------------------------------------------------
// Mistake 40: Assuming deterministic parallel execution
// -----------------------------------------------------------------------------

TEST_CASE("Accumulator combination order", "[mistake][order]") {
    // Multiple accumulators combine in unspecified order
    // For associative+commutative ops, this is fine

    auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 20, 4) {
        return i;
    } ILP_END_REDUCE;

    REQUIRE(result == 190);  // Sum is always correct
}
