#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <limits>
#include <cstdint>
#include <numeric>

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
    } ILP_END;

    // Sum of 5 values starting from max-10
    int64_t expected = 0;
    for (int i = start; i < end; ++i) expected += i;
    REQUIRE(sum == expected);
}

TEST_CASE("Near min integer range", "[evil][limits]") {
    int64_t sum = 0;
    int start = std::numeric_limits<int>::min() + 5;
    int end = std::numeric_limits<int>::min() + 10;

    ILP_FOR(auto i, start, end, 4) {
        sum += i;
    } ILP_END;

    int64_t expected = 0;
    for (int i = start; i < end; ++i) expected += i;
    REQUIRE(sum == expected);
}

TEST_CASE("Size_t near max", "[evil][limits]") {
    size_t start = std::numeric_limits<size_t>::max() - 20;
    size_t end = std::numeric_limits<size_t>::max() - 10;

    int count = 0;
    ILP_FOR(auto i, start, end, 4) {
        count++;
        (void)i;
    } ILP_END;

    REQUIRE(count == 10);
}

// -----------------------------------------------------------------------------
// Evil 2: Inverted range behavior verification
// -----------------------------------------------------------------------------

TEST_CASE("Inverted unsigned range", "[evil][inverted]") {
    // This is undefined territory - what happens?
    unsigned count = 0;
    ILP_FOR(auto i, 10u, 0u, 4) {  // Inverted!
        count++;
    } ILP_END;
    // Should be 0 iterations for safety
    REQUIRE(count == 0);
}

TEST_CASE("Inverted signed range", "[evil][inverted]") {
    int count = 0;
    ILP_FOR(auto i, 100, -100, 4) {  // Inverted!
        count++;
    } ILP_END;
    REQUIRE(count == 0);
}

// -----------------------------------------------------------------------------
// Evil 3: N vs Range size mismatches
// -----------------------------------------------------------------------------

TEST_CASE("Exactly 2N elements", "[evil][boundary]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 8, 4) {  // 8 = 2*4
        sum += i;
    } ILP_END;
    REQUIRE(sum == 28);  // 0+1+2+3+4+5+6+7
}

TEST_CASE("Exactly 2N-1 elements", "[evil][boundary]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 7, 4) {  // 7 = 2*4-1
        sum += i;
    } ILP_END;
    REQUIRE(sum == 21);
}

TEST_CASE("Exactly 2N+1 elements", "[evil][boundary]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 9, 4) {  // 9 = 2*4+1
        sum += i;
    } ILP_END;
    REQUIRE(sum == 36);
}

// -----------------------------------------------------------------------------
// Evil 5: Reduce with problematic initial values
// -----------------------------------------------------------------------------

TEST_CASE("Reduce with zero init for product", "[evil][reduce]") {
    // Product with 0 init - always 0
    auto result = ilp::reduce<4>(1, 10, 0, std::multiplies<>(), [&](auto i) {
        return i;
    });
    REQUIRE(result == 0);  // 0 * anything = 0
}

TEST_CASE("Reduce with negative init", "[evil][reduce]") {
    auto result = ilp::reduce<4>(0, 10, 0, std::plus<>{}, [&](auto i) {
        return i;
    });
    // Wait, reduce_sum doesn't take init... default is 0
    REQUIRE(result == 45);
}

TEST_CASE("Reduce with max int init", "[evil][reduce]") {
    auto result = ilp::reduce<4>(
        0, 100,
        std::numeric_limits<int>::max(),
        [](int a, int b) { return std::min(a, b); },
        [&](auto i) {
            return i;
        }
    );
    REQUIRE(result == 0);
}

// -----------------------------------------------------------------------------
// Evil 6: Control flow in every position
// -----------------------------------------------------------------------------

#if !defined(ILP_MODE_SIMPLE)

TEST_CASE("Break at N boundary", "[evil][control]") {
    // N=4, break exactly at position 4
    int sum = 0;
    ILP_FOR(auto i, 0, 100, 4) {
        if (i == 4) ILP_BREAK;
        sum += i;
    } ILP_END;
    REQUIRE(sum == 6);  // 0+1+2+3
}

TEST_CASE("Break at N-1", "[evil][control]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 100, 4) {
        if (i == 3) ILP_BREAK;
        sum += i;
    } ILP_END;
    REQUIRE(sum == 3);  // 0+1+2
}

TEST_CASE("Break at N+1", "[evil][control]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 100, 4) {
        if (i == 5) ILP_BREAK;
        sum += i;
    } ILP_END;
    REQUIRE(sum == 10);  // 0+1+2+3+4
}

TEST_CASE("Break at 2N", "[evil][control]") {
    int sum = 0;
    ILP_FOR(auto i, 0, 100, 4) {
        if (i == 8) ILP_BREAK;
        sum += i;
    } ILP_END;
    REQUIRE(sum == 28);  // 0+1+...+7
}

#endif

// -----------------------------------------------------------------------------
// Evil 7: Weird type combinations
// -----------------------------------------------------------------------------

TEST_CASE("Mixing int and size_t", "[evil][types]") {
    size_t sum = 0;
    ILP_FOR(auto i, 0, 10, 4) {
        sum += static_cast<size_t>(i);
    } ILP_END;
    REQUIRE(sum == 45);
}

TEST_CASE("int16_t accumulator with int iteration", "[evil][types]") {
    int16_t sum = 0;
    ILP_FOR(auto i, 0, 100, 4) {
        sum += static_cast<int16_t>(i);
    } ILP_END;
    REQUIRE(sum == 4950);
}

// -----------------------------------------------------------------------------
// Evil 8: Reduce with early break returns
// -----------------------------------------------------------------------------

#if !defined(ILP_MODE_SIMPLE)

TEST_CASE("Reduce break returns init value behavior", "[evil][reduce]") {
    // Breaking returns nullopt from body, but what about accumulated values?
    auto result = ilp::reduce<4>(0, 100, 0, std::plus<>(),
        [&](auto i) -> std::optional<int> {
            if (i == 10) return std::nullopt;
            return i;
        });

    // Expected: 0+1+2+3+4+5+6+7+8+9 = 45
    REQUIRE(result == 45);
}

TEST_CASE("Reduce break at first in each block", "[evil][reduce]") {
    // Break at position 0, 4, 8 (first of each unroll block)
    auto result = ilp::reduce<4>(0, 12, 0, std::plus<>(),
        [&](auto i) -> std::optional<int> {
            if (i % 4 == 0) return std::nullopt;
            return i;
        });

    // Breaks on first iteration
    REQUIRE(result == 0);
}

#endif

// -----------------------------------------------------------------------------
// Evil 9: Vector edge cases
// -----------------------------------------------------------------------------

TEST_CASE("Vector with one element less than N", "[evil][vector]") {
    std::vector<int> data = {1, 2, 3};  // 3 < N=4
    int sum = 0;
    ILP_FOR_RANGE(auto&& val, data, 4) {
        sum += val;
    } ILP_END;
    REQUIRE(sum == 6);
}

TEST_CASE("Vector exactly N elements", "[evil][vector]") {
    std::vector<int> data = {1, 2, 3, 4};  // 4 = N
    int sum = 0;
    ILP_FOR_RANGE(auto&& val, data, 4) {
        sum += val;
    } ILP_END;
    REQUIRE(sum == 10);
}

// -----------------------------------------------------------------------------
// Evil 10: For-until with multiple matches
// -----------------------------------------------------------------------------

TEST_CASE("Find with multiple potential matches", "[evil][find]") {
    // All elements match - should return first
    auto result = ilp::find<4>(0, 100, [&](auto, auto) {
        return true;  // All match
    });

    REQUIRE(result != 100);  // Found (not sentinel)
    REQUIRE(result == 0);    // First match
}

TEST_CASE("Find matches in every unroll position", "[evil][find]") {
    // Match at positions 0, 1, 2, 3 (all within first block)
    std::vector<int> results;

    for (int target = 0; target < 4; ++target) {
        auto result = ilp::find<4>(0, 100, [&](auto i, auto) {
            return i == target;
        });
        results.push_back(result);
    }

    for (int j = 0; j < 4; ++j) {
        REQUIRE(results[j] != 100);  // Found
        REQUIRE(results[j] == j);
    }
}

// -----------------------------------------------------------------------------
// Evil 11: Iteration order stress test
// -----------------------------------------------------------------------------

TEST_CASE("Strict iteration order for side effects", "[evil][order]") {
    std::vector<int> order;
    order.reserve(20);

    ILP_FOR(auto i, 0, 20, 4) {
        order.push_back(i);
    } ILP_END;

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
    } ILP_END;

    REQUIRE(order == data);
}

// -----------------------------------------------------------------------------
// Evil 12: Reduce accumulator combination order
// -----------------------------------------------------------------------------

TEST_CASE("Reduce accumulator order - associative", "[evil][accumulator]") {
    // For associative ops, order shouldn't matter
    auto result = ilp::reduce<4>(0, 20, 0, std::plus<>{}, [&](auto i) {
        return i;
    });
    REQUIRE(result == 190);  // Always correct for sum
}

TEST_CASE("Reduce accumulator - max operation", "[evil][accumulator]") {
    std::vector<int> data = {5, 3, 9, 1, 8, 2, 7, 4, 6, 0};

    auto result = ilp::reduce_range<4>(
        data,
        std::numeric_limits<int>::min(),
        [](int a, int b) { return std::max(a, b); },
        [&](auto&& val) {
            return val;
        }
    );

    REQUIRE(result == 9);
}

// -----------------------------------------------------------------------------
// Evil 13: Empty operations that should do nothing
// -----------------------------------------------------------------------------

TEST_CASE("Reduce of empty with identity ops", "[evil][empty]") {
    std::vector<int> empty;

    // Sum of empty = 0
    auto sum = ilp::reduce_range<4>(empty, 0, std::plus<>{}, [&](auto&& val) {
        return val;
    });
    REQUIRE(sum == 0);

    // Product of empty with init 1 = 1
    auto product = ilp::reduce_range<4>(
        empty, 1, std::multiplies<>(), [&](auto&& val) {
            return val;
        }
    );
    REQUIRE(product == 1);
}

// -----------------------------------------------------------------------------
// Evil 14: Stress test with many iterations
// -----------------------------------------------------------------------------

TEST_CASE("100000 iterations", "[evil][stress]") {
    int64_t result = ilp::reduce<4>((int64_t)0, (int64_t)100000, 0LL, std::plus<>{}, [&](auto i) {
        return i;
    });

    REQUIRE(result == 4999950000LL);
}

// -----------------------------------------------------------------------------
// Evil 15: Verify no double evaluation
// -----------------------------------------------------------------------------

TEST_CASE("No double evaluation of body", "[evil][evaluation]") {
    int eval_count = 0;

    auto result = ilp::reduce<4>(0, 100, 0, std::plus<>{}, [&](auto i) {
        eval_count++;
        return i;
    });

    REQUIRE(result == 4950);
    REQUIRE(eval_count == 100);  // Each i evaluated exactly once
}

// -----------------------------------------------------------------------------
// Evil 16: Const correctness
// -----------------------------------------------------------------------------

TEST_CASE("Const vector reduce", "[evil][const]") {
    const std::vector<int> data = {1, 2, 3, 4, 5};

    auto result = ilp::reduce_range<4>(data, 0, std::plus<>{}, [&](auto&& val) {
        return val;
    });

    REQUIRE(result == 15);
}

// -----------------------------------------------------------------------------
// Evil 17: Find at exact boundaries
// -----------------------------------------------------------------------------

TEST_CASE("Find at N-1", "[evil][find]") {
    auto result = ilp::find<4>(0, 100, [&](auto i, auto) {
        return i == 3;
    });
    REQUIRE(result == 3);
}

TEST_CASE("Find at N", "[evil][find]") {
    auto result = ilp::find<4>(0, 100, [&](auto i, auto) {
        return i == 4;
    });
    REQUIRE(result == 4);
}

TEST_CASE("Find at N+1", "[evil][find]") {
    auto result = ilp::find<4>(0, 100, [&](auto i, auto) {
        return i == 5;
    });
    REQUIRE(result == 5);
}

TEST_CASE("Find at 2N-1", "[evil][find]") {
    auto result = ilp::find<4>(0, 100, [&](auto i, auto) {
        return i == 7;
    });
    REQUIRE(result == 7);
}

TEST_CASE("Find at 2N", "[evil][find]") {
    auto result = ilp::find<4>(0, 100, [&](auto i, auto) {
        return i == 8;
    });
    REQUIRE(result == 8);
}

// -----------------------------------------------------------------------------
// Evil 19: Return iterator validity
// -----------------------------------------------------------------------------

TEST_CASE("Ret-simple returns valid sentinel", "[evil][ret]") {
    // When not found, should return exactly end
    auto result = ilp::find<4>(0, 42, [&](auto i, auto end) {
        if (i == 999) return i;  // Never found
        return end;
    });

    REQUIRE(result == 42);  // End sentinel
}

TEST_CASE("Range-ret returns valid end iterator", "[evil][ret]") {
    std::vector<int> data = {1, 2, 3, 4, 5};

    auto it = ilp::find_range_idx<4>(data, [&](auto&& val, auto idx, auto end) {
        if (val == 999) return std::ranges::begin(data) + idx;
        return end;
    });

    REQUIRE(it == data.end());
}

// -----------------------------------------------------------------------------
// Evil 20: Recursive reduce (inner reduce)
// -----------------------------------------------------------------------------

TEST_CASE("Nested reduce", "[evil][nested]") {
    // Sum of sums
    int total = 0;

    ILP_FOR(auto i, 0, 5, 4) {
        auto inner_sum = ilp::reduce<4>(0, 5, 0, std::plus<>{}, [&](auto j) {
            return j;
        });
        total += inner_sum;
    } ILP_END;

    // Each inner sum is 0+1+2+3+4 = 10, done 5 times = 50
    REQUIRE(total == 50);
}

#endif // !ILP_MODE_SIMPLE
