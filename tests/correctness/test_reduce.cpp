#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <numeric>
#include <cstdint>

#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)

TEST_CASE("Reduce with Break", "[reduce][control]") {
    SECTION("Range-based reduce with break stops correctly") {
        // We want to sum the elements of a vector, but stop when we hit a value > 5.
        // The unroll factor is 4. The break should happen in the second unrolled block.
        std::vector<int> data = {1, 1, 1, 1,   // Block 1 (sum = 4)
                                 1, 1, 9, 1,   // Block 2 (break at 9)
                                 1, 1, 1, 1};  // Should not be processed

        // The body of the reduce lambda returns the value of the element.
        // The reduction operation is addition.
        // The break happens when it processes the element '9'.
        // Before the bug fix, the loop would continue, and the final result would be incorrect.
        // After the bug fix, the loop should stop, and the final reduction should be over the
        // accumulators holding the values processed before the break.
        auto result = ILP_REDUCE_RANGE(std::plus<int>(), 0, auto&& val, data, 4) {
            if (val > 5) {
                ILP_REDUCE_BREAK;
            }
            ILP_REDUCE_RETURN(val);
        } ILP_END_REDUCE;

        // The elements processed should be 1, 1, 1, 1, 1, 1. The sum is 6.
        // The element '9' triggers the break. The 'return val' for '9' might or might not
        // be added to an accumulator depending on the exact sequence of operations.
        // Let's trace the buggy version:
        // accs = {0,0,0,0}
        // Block 1: accs = {1,1,1,1}
        // Block 2:
        //   - accs[0] += 1 (val=1) -> accs[0]=2
        //   - accs[1] += 1 (val=1) -> accs[1]=2
        //   - val=9, break! The lambda for 9 returns, ctrl.ok=false. Short-circuit stops next.
        //   - The lambda for val=1 is skipped.
        // Loop continues!
        // Block 3: This would run in the buggy version.
        // Final reduction: buggy result would be large.
        //
        // Tracing the fixed version:
        // accs = {0,0,0,0}
        // Block 1: accs = {1,1,1,1}
        // Block 2:
        //   - accs[0] += 1 (val=1) -> accs[0]=2
        //   - accs[1] += 1 (val=1) -> accs[1]=2
        //   - val=9, ILP_BREAK. The lambda for 9 returns, ctrl.ok=false.
        //   - The lambda for the next val=1 is skipped due to short-circuiting.
        // The `if (!ctrl.ok) break;` is hit, so the outer loop terminates.
        // Final reduction: accs[0]+accs[1]+accs[2]+accs[3] = 2+2+1+1 = 6. Correct.

        REQUIRE(result == 6);
    }

    SECTION("Index-based reduce with break stops correctly") {
        // Sum numbers up to a value where the index triggers a break.
        auto result = ILP_REDUCE(std::plus<int>(), 0, auto i, 0, 100, 4) {
            if (i >= 10) {
                ILP_REDUCE_BREAK;
            }
            ILP_REDUCE_RETURN((int)i);
        } ILP_END_REDUCE;

        // The sum should be 0 + 1 + ... + 9 = 45
        int expected_sum = 0;
        for(int i = 0; i < 10; ++i) {
            expected_sum += i;
        }

        REQUIRE(result == expected_sum);
    }
}

TEST_CASE("Simple Reduce", "[reduce]") {
    SECTION("Sum of 0 to 99") {
        auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 100, 4) {
            return static_cast<int64_t>(i);
        } ILP_END_REDUCE;

        int64_t expected = 0;
        for(int i = 0; i < 100; ++i) expected += i;

        REQUIRE(result == expected);
    }

    SECTION("Sum of a vector") {
        std::vector<int> data(100);
        std::iota(data.begin(), data.end(), 0);

        auto result = ILP_REDUCE_RANGE(std::plus<>{}, 0, auto&& val, data, 8) {
            return static_cast<int64_t>(val);
        } ILP_END_REDUCE;

        int64_t expected = 0;
        for(int val : data) expected += val;

        REQUIRE(result == expected);
    }
}

TEST_CASE("Bitwise Reduce Operations", "[reduce][bitwise]") {
    SECTION("Bitwise AND reduction") {
        // Test bit_and with a range
        std::vector<unsigned> data = {0xFF, 0xF0, 0x3F, 0x0F};

        auto result = ILP_REDUCE_RANGE(std::bit_and<>(), 0xFF, auto&& val, data, 4) {
            return val;
        } ILP_END_REDUCE;

        unsigned expected = 0xFF;
        for(auto v : data) expected &= v;

        REQUIRE(result == expected);
        REQUIRE(result == 0x00);  // All bits should be cleared
    }

    SECTION("Bitwise OR reduction") {
        // Test bit_or with a range
        std::vector<unsigned> data = {0x01, 0x02, 0x04, 0x08};

        auto result = ILP_REDUCE_RANGE(std::bit_or<>(), 0, auto&& val, data, 4) {
            return val;
        } ILP_END_REDUCE;

        unsigned expected = 0;
        for(auto v : data) expected |= v;

        REQUIRE(result == expected);
        REQUIRE(result == 0x0F);  // All lower nibble bits set
    }

    SECTION("Bitwise XOR reduction") {
        // Test bit_xor - useful for parity checks
        std::vector<unsigned> data = {0xFF, 0xFF, 0x0F, 0x0F};

        auto result = ILP_REDUCE_RANGE(std::bit_xor<>(), 0, auto&& val, data, 4) {
            return val;
        } ILP_END_REDUCE;

        unsigned expected = 0;
        for(auto v : data) expected ^= v;

        REQUIRE(result == expected);
        REQUIRE(result == 0x00);  // XOR of pairs cancels out
    }

    SECTION("Bitwise AND with index-based loop") {
        // Create a bitmask by ANDing values
        auto result = ILP_REDUCE(std::bit_and<>(), 0xFFFFFFFF, auto i, 0, 10, 4) {
            return ~(1u << i);  // Clear bit i
        } ILP_END_REDUCE;

        unsigned expected = 0xFFFFFFFF;
        for(unsigned i = 0; i < 10; ++i) {
            expected &= ~(1u << i);
        }

        REQUIRE(result == expected);
    }
}

TEST_CASE("Cleanup loops with remainders", "[reduce][cleanup]") {
    SECTION("Range reduce with remainder hits cleanup") {
        // 9 elements with unroll 4: hits cleanup loop for last element
        std::vector<int> data = {1, 1, 1, 1, 1, 1, 1, 1, 1};

        auto result = ILP_REDUCE_RANGE(std::plus<>{}, 0, auto&& val, data, 4) {
            return val;
        } ILP_END_REDUCE;

        REQUIRE(result == 9);
    }

    SECTION("Range reduce SIMPLE with std::plus<>") {
        std::vector<int> data = {1, 2, 3, 4, 5};

        auto result = ILP_REDUCE_RANGE(std::plus<>(), 0, auto&& val, data, 4) {
            return val;
        } ILP_END_REDUCE;

        REQUIRE(result == 15);
    }

    SECTION("Index reduce SIMPLE with std::plus<>") {
        auto result = ILP_REDUCE(std::plus<>(), 0, auto i, 0, 10, 4) {
            return i;
        } ILP_END_REDUCE;

        REQUIRE(result == 45);
    }

    SECTION("Vector<double> reduce SIMPLE with std::plus<>") {
        std::vector<double> data = {1.5, 2.5, 3.5, 4.5, 5.5};

        auto result = ILP_REDUCE_RANGE(std::plus<>(), 0.0, auto&& val, data, 4) {
            return val;
        } ILP_END_REDUCE;

        REQUIRE(result == 17.5);
    }

    SECTION("Array<int, 7> reduce SIMPLE with std::plus<>") {
        std::array<int, 7> data = {1, 2, 3, 4, 5, 6, 7};

        auto result = ILP_REDUCE_RANGE(std::plus<>(), 0, auto&& val, data, 4) {
            return val;
        } ILP_END_REDUCE;

        REQUIRE(result == 28);
    }

    SECTION("Unsigned int reduce SIMPLE with std::plus<> and N=4") {
        auto result = ILP_REDUCE(std::plus<>(), 0u, auto i, 0u, 10u, 4) {
            return i;
        } ILP_END_REDUCE;

        REQUIRE(result == 45u);
    }

    SECTION("Int reduce SIMPLE with std::plus<> and N=1") {
        auto result = ILP_REDUCE(std::plus<>(), 0, auto i, 0, 10, 1) {
            return i;
        } ILP_END_REDUCE;

        REQUIRE(result == 45);
    }

    SECTION("Index reduce with remainder") {
        // 11 iterations with unroll 4: 11 = 2*4 + 3
        auto result = ILP_REDUCE(std::plus<>{}, 0, auto i, 0, 11, 4) {
            return 1;
        } ILP_END_REDUCE;

        REQUIRE(result == 11);
    }

    SECTION("Reduce with break in cleanup loop") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9};

        auto result = ILP_REDUCE_RANGE(std::plus<>(), 0, auto&& val, data, 4) {
            if (val == 9) {  // Last element, in cleanup
                ILP_REDUCE_BREAK;
            }
            ILP_REDUCE_RETURN(val);
        } ILP_END_REDUCE;

        // Sum 1+2+3+4+5+6+7+8 = 36
        REQUIRE(result == 36);
    }

    SECTION("FOR_RANGE_RET cleanup loop") {
        // Test lines 383-384: range-based FOR_RET with remainder
        auto helper = [](const std::vector<int>& data) -> std::optional<int> {
            ILP_FOR_RANGE_RET(int, auto&& val, data, 4) {
                if (val == 7) {  // Last element in cleanup loop
                    ILP_RETURN(val * 10);
                }
            } ILP_END_RET;
            return std::nullopt;
        };

        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7};  // 7 elements, unroll 4
        auto result = helper(data);

        REQUIRE(result.has_value());
        REQUIRE(result.value() == 70);
    }
}

// =============================================================================
// Path detection tests - verify ReduceResult detection
// =============================================================================

TEST_CASE("ReduceResult is detected with ILP_REDUCE_RETURN", "[reduce][path]") {
    using namespace ilp::detail;

    // ReduceResult<T> has value and _break fields
    auto lambda = [](int i, ilp::LoopCtrl<void>&) {
        return ReduceResult<int>{i, false};
    };
    using R = std::invoke_result_t<decltype(lambda), int, ilp::LoopCtrl<void>&>;
    static_assert(is_reduce_result_v<R>, "Should detect ReduceResult");

    auto result = ILP_REDUCE_AUTO(std::plus<>{}, 0, auto i, 0, 10) {
        ILP_REDUCE_RETURN(i);
    } ILP_END_REDUCE;
    CHECK(result == 45);
}

TEST_CASE("ReduceResult with break stops correctly", "[reduce][path]") {
    using namespace ilp::detail;

    auto result = ILP_REDUCE_AUTO(std::plus<>{}, 0, auto i, 0, 100) {
        if (i >= 10) ILP_REDUCE_BREAK;
        ILP_REDUCE_RETURN(i);
    } ILP_END_REDUCE;
    CHECK(result == 45);  // 0+1+...+9
}

TEST_CASE("Range-based reduce with ILP_REDUCE_RETURN", "[reduce][path][range]") {
    using namespace ilp::detail;
    std::vector<int> data = {0, 1, 2, 3, 4};

    auto result = ILP_REDUCE_RANGE_AUTO(std::plus<>{}, 0, auto val, data) {
        ILP_REDUCE_RETURN(val);
    } ILP_END_REDUCE;
    CHECK(result == 10);
}

TEST_CASE("Range-based reduce with break stops correctly", "[reduce][path][range]") {
    using namespace ilp::detail;
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    auto result = ILP_REDUCE_RANGE_AUTO(std::plus<>{}, 0, auto val, data) {
        if (val >= 5) ILP_REDUCE_BREAK;
        ILP_REDUCE_RETURN(val);
    } ILP_END_REDUCE;
    CHECK(result == 10);  // 0+1+2+3+4
}

#endif