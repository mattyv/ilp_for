#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <numeric>

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
        auto result = ILP_REDUCE_RANGE(std::plus<int>(), 0, val, data, 4) {
            if (val > 5) {
                ILP_BREAK_RET(0);
            }
            return val;
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
        auto result = ILP_REDUCE(std::plus<int>(), 0, i, 0, 100, 4) {
            if (i >= 10) {
                ILP_BREAK_RET(0);
            }
            return (int)i;
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
        auto result = ILP_REDUCE_SUM(i, 0, 100, 4) {
            return (unsigned)i;
        } ILP_END_REDUCE;

        unsigned expected = 0;
        for(unsigned i = 0; i < 100; ++i) expected += i;

        REQUIRE(result == expected);
    }

    SECTION("Sum of a vector") {
        std::vector<int> data(100);
        std::iota(data.begin(), data.end(), 0);

        auto result = ILP_REDUCE_RANGE_SUM(val, data, 8) {
            return val;
        } ILP_END_REDUCE;

        long long expected = 0;
        for(int val : data) expected += val;

        REQUIRE(result == expected);
    }
}

#endif