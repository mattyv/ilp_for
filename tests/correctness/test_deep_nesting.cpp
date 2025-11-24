#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>

// =============================================================================
// DEEP NESTING STRESS TESTS
// =============================================================================

// -----------------------------------------------------------------------------
// Deeply Nested Index Loops
// -----------------------------------------------------------------------------

TEST_CASE("4 levels nested index loops", "[nesting][deep]") {
    int count = 0;

    ILP_FOR_SIMPLE(i, 0, 3, 4) {
        ILP_FOR_SIMPLE(j, 0, 3, 4) {
            ILP_FOR_SIMPLE(k, 0, 3, 4) {
                ILP_FOR_SIMPLE(l, 0, 3, 4) {
                    count++;
                } ILP_END;
            } ILP_END;
        } ILP_END;
    } ILP_END;

    REQUIRE(count == 81);  // 3^4
}

TEST_CASE("5 levels nested index loops", "[nesting][deep]") {
    int count = 0;

    ILP_FOR_SIMPLE(a, 0, 2, 4) {
        ILP_FOR_SIMPLE(b, 0, 2, 4) {
            ILP_FOR_SIMPLE(c, 0, 2, 4) {
                ILP_FOR_SIMPLE(d, 0, 2, 4) {
                    ILP_FOR_SIMPLE(e, 0, 2, 4) {
                        count++;
                    } ILP_END;
                } ILP_END;
            } ILP_END;
        } ILP_END;
    } ILP_END;

    REQUIRE(count == 32);  // 2^5
}

TEST_CASE("6 levels nested index loops", "[nesting][deep]") {
    int count = 0;

    ILP_FOR_SIMPLE(a, 0, 2, 4) {
        ILP_FOR_SIMPLE(b, 0, 2, 4) {
            ILP_FOR_SIMPLE(c, 0, 2, 4) {
                ILP_FOR_SIMPLE(d, 0, 2, 4) {
                    ILP_FOR_SIMPLE(e, 0, 2, 4) {
                        ILP_FOR_SIMPLE(f, 0, 2, 4) {
                            count++;
                        } ILP_END;
                    } ILP_END;
                } ILP_END;
            } ILP_END;
        } ILP_END;
    } ILP_END;

    REQUIRE(count == 64);  // 2^6
}

// -----------------------------------------------------------------------------
// Nested Loops with Variable Capture from Outer Scope
// -----------------------------------------------------------------------------

TEST_CASE("Nested with outer variable accumulation", "[nesting][capture]") {
    int total = 0;

    ILP_FOR_SIMPLE(i, 0, 5, 4) {
        int inner_sum = 0;
        ILP_FOR_SIMPLE(j, 0, 5, 4) {
            inner_sum += j;
        } ILP_END;
        total += inner_sum * i;
    } ILP_END;

    // inner_sum always = 10, total = 10*0 + 10*1 + 10*2 + 10*3 + 10*4 = 100
    REQUIRE(total == 100);
}

TEST_CASE("Nested with complex expression", "[nesting][capture]") {
    int sum = 0;

    ILP_FOR_SIMPLE(i, 0, 5, 4) {
        ILP_FOR_SIMPLE(j, 0, 5, 4) {
            ILP_FOR_SIMPLE(k, 0, 5, 4) {
                sum += i * j * k;
            } ILP_END;
        } ILP_END;
    } ILP_END;

    // Sum of i*j*k for all i,j,k in [0,5)
    int expected = 0;
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 5; ++j)
            for (int k = 0; k < 5; ++k)
                expected += i * j * k;

    REQUIRE(sum == expected);
}

// -----------------------------------------------------------------------------
// Nested Range Loops
// -----------------------------------------------------------------------------

TEST_CASE("Nested range loops", "[nesting][range]") {
    std::vector<std::vector<int>> matrix = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };

    int sum = 0;
    ILP_FOR_RANGE_SIMPLE(row, matrix, 4) {
        ILP_FOR_RANGE_SIMPLE(val, row, 4) {
            sum += val;
        } ILP_END;
    } ILP_END;

    REQUIRE(sum == 45);  // 1+2+...+9
}

TEST_CASE("3-level nested range loops", "[nesting][range]") {
    std::vector<std::vector<std::vector<int>>> cube = {
        {{1, 2}, {3, 4}},
        {{5, 6}, {7, 8}}
    };

    int sum = 0;
    ILP_FOR_RANGE_SIMPLE(plane, cube, 4) {
        ILP_FOR_RANGE_SIMPLE(row, plane, 4) {
            ILP_FOR_RANGE_SIMPLE(val, row, 4) {
                sum += val;
            } ILP_END;
        } ILP_END;
    } ILP_END;

    REQUIRE(sum == 36);  // 1+2+...+8
}

// -----------------------------------------------------------------------------
// Mixed Index and Range Nesting
// -----------------------------------------------------------------------------

TEST_CASE("Index outer, range inner", "[nesting][mixed]") {
    std::vector<int> data = {1, 2, 3, 4, 5};

    int total = 0;
    ILP_FOR_SIMPLE(multiplier, 1, 4, 4) {
        ILP_FOR_RANGE_SIMPLE(val, data, 4) {
            total += val * multiplier;
        } ILP_END;
    } ILP_END;

    // Sum 1..5 = 15, multipliers 1,2,3 -> 15*1 + 15*2 + 15*3 = 90
    REQUIRE(total == 90);
}

TEST_CASE("Range outer, index inner", "[nesting][mixed]") {
    std::vector<int> bases = {1, 10, 100};

    int total = 0;
    ILP_FOR_RANGE_SIMPLE(base, bases, 4) {
        ILP_FOR_SIMPLE(i, 0, 5, 4) {
            total += base + i;
        } ILP_END;
    } ILP_END;

    // (1+0)+(1+1)+(1+2)+(1+3)+(1+4) + same for 10 and 100
    // = (5 + 10) + (50 + 10) + (500 + 10) = 585
    REQUIRE(total == 585);
}

// -----------------------------------------------------------------------------
// Nested Reduce Operations
// -----------------------------------------------------------------------------

TEST_CASE("Nested reduce - sum of products", "[nesting][reduce]") {
    int total = 0;

    ILP_FOR_SIMPLE(i, 1, 5, 4) {
        auto product = ILP_REDUCE_SIMPLE(
            std::multiplies<>(), 1, j, 1, 4, 4
        ) {
            return j;
        } ILP_END_REDUCE;
        total += product * i;
    } ILP_END;

    // product = 1*2*3 = 6 for each i
    // total = 6*1 + 6*2 + 6*3 + 6*4 = 60
    REQUIRE(total == 60);
}

TEST_CASE("Nested reduce - matrix sum", "[nesting][reduce]") {
    std::vector<std::vector<int>> matrix = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };

    auto result = ILP_REDUCE_RANGE_SUM(row, matrix, 4) {
        return ILP_REDUCE_RANGE_SUM(val, row, 4) {
            return val;
        } ILP_END_REDUCE;
    } ILP_END_REDUCE;

    REQUIRE(result == 45);
}

// -----------------------------------------------------------------------------
// Nested with Control Flow
// -----------------------------------------------------------------------------

#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)

TEST_CASE("Nested loops - break inner", "[nesting][control]") {
    int count = 0;

    ILP_FOR_SIMPLE(i, 0, 5, 4) {
        ILP_FOR(j, 0, 10, 4) {
            if (j >= 3) ILP_BREAK;
            count++;
        } ILP_END;
    } ILP_END;

    REQUIRE(count == 15);  // 5 outer * 3 inner
}

TEST_CASE("Nested loops - continue inner", "[nesting][control]") {
    int sum = 0;

    ILP_FOR_SIMPLE(i, 0, 3, 4) {
        ILP_FOR(j, 0, 10, 4) {
            if (j % 2 == 0) ILP_CONTINUE;
            sum += j;
        } ILP_END;
    } ILP_END;

    // 1+3+5+7+9 = 25, done 3 times = 75
    REQUIRE(sum == 75);
}

#endif

// -----------------------------------------------------------------------------
// Nested For-Until
// -----------------------------------------------------------------------------

TEST_CASE("Nested for-until - find in matrix", "[nesting][until]") {
    std::vector<std::vector<int>> matrix = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9}
    };

    int found_row = -1;
    int found_col = -1;
    int target = 5;

    int row_idx = 0;
    bool found = false;
    ILP_FOR_RANGE_SIMPLE(row, matrix, 4) {
        if (!found) {
            int col_idx = 0;
            auto col_result = ILP_FOR_UNTIL_RANGE(val, row, 4) {
                if (val == target) {
                    found_col = col_idx;
                    return true;
                }
                col_idx++;
                return false;
            } ILP_END_UNTIL;

            if (col_result.has_value()) {
                found_row = row_idx;
                found = true;
            }
        }
        row_idx++;
    } ILP_END;

    REQUIRE(found_row == 1);
    REQUIRE(found_col == 1);
}

// -----------------------------------------------------------------------------
// Stress: Many Iterations in Nested Loops
// -----------------------------------------------------------------------------

TEST_CASE("Nested loops - 10x10x10 iterations", "[nesting][stress]") {
    int count = 0;

    ILP_FOR_SIMPLE(i, 0, 10, 4) {
        ILP_FOR_SIMPLE(j, 0, 10, 4) {
            ILP_FOR_SIMPLE(k, 0, 10, 4) {
                count++;
            } ILP_END;
        } ILP_END;
    } ILP_END;

    REQUIRE(count == 1000);
}

TEST_CASE("Nested loops with accumulation - 10x10x10", "[nesting][stress]") {
    int64_t sum = 0;

    ILP_FOR_SIMPLE(i, 0, 10, 4) {
        ILP_FOR_SIMPLE(j, 0, 10, 4) {
            ILP_FOR_SIMPLE(k, 0, 10, 4) {
                sum += i + j + k;
            } ILP_END;
        } ILP_END;
    } ILP_END;

    // Each coordinate appears 100 times, sum of 0..9 = 45
    // Total contribution per dimension = 45 * 100 = 4500
    // Total = 4500 * 3 = 13500
    REQUIRE(sum == 13500);
}

// -----------------------------------------------------------------------------
// Different N Values at Each Level
// -----------------------------------------------------------------------------

TEST_CASE("Different N at each nesting level", "[nesting][varied]") {
    int count = 0;

    ILP_FOR_SIMPLE(i, 0, 5, 8) {    // N=8
        ILP_FOR_SIMPLE(j, 0, 5, 4) {  // N=4
            ILP_FOR_SIMPLE(k, 0, 5, 2) {  // N=2
                ILP_FOR_SIMPLE(l, 0, 5, 1) {  // N=1
                    count++;
                } ILP_END;
            } ILP_END;
        } ILP_END;
    } ILP_END;

    REQUIRE(count == 625);  // 5^4
}

// -----------------------------------------------------------------------------
// Nested with Reductions at Each Level
// -----------------------------------------------------------------------------

TEST_CASE("Reduce at each nesting level", "[nesting][reduce]") {
    auto level4 = ILP_REDUCE_SUM(i, 0, 3, 4) {
        auto level3 = ILP_REDUCE_SUM(j, 0, 3, 4) {
            auto level2 = ILP_REDUCE_SUM(k, 0, 3, 4) {
                auto level1 = ILP_REDUCE_SUM(l, 0, 3, 4) {
                    return l;
                } ILP_END_REDUCE;
                return level1;
            } ILP_END_REDUCE;
            return level2;
        } ILP_END_REDUCE;
        return level3;
    } ILP_END_REDUCE;

    // Sum 0..2 = 3 at each level, multiplied by number of iterations
    // 3 * 3 * 3 * 3 = 81
    REQUIRE(level4 == 81);
}

// -----------------------------------------------------------------------------
// Nested Loops Building Data Structures
// -----------------------------------------------------------------------------

TEST_CASE("Building matrix with nested loops", "[nesting][build]") {
    std::vector<std::vector<int>> matrix;
    matrix.reserve(5);

    ILP_FOR_SIMPLE(i, 0, 5, 4) {
        std::vector<int> row;
        row.reserve(5);
        ILP_FOR_SIMPLE(j, 0, 5, 4) {
            row.push_back(i * 5 + j);
        } ILP_END;
        matrix.push_back(std::move(row));
    } ILP_END;

    REQUIRE(matrix.size() == 5);
    REQUIRE(matrix[0].size() == 5);
    REQUIRE(matrix[0][0] == 0);
    REQUIRE(matrix[4][4] == 24);
}

// -----------------------------------------------------------------------------
// Edge: Empty Inner Loop
// -----------------------------------------------------------------------------

TEST_CASE("Empty inner loop", "[nesting][edge]") {
    int outer_count = 0;
    int inner_count = 0;

    ILP_FOR_SIMPLE(i, 0, 5, 4) {
        outer_count++;
        ILP_FOR_SIMPLE(j, 0, 0, 4) {  // Empty!
            inner_count++;
        } ILP_END;
    } ILP_END;

    REQUIRE(outer_count == 5);
    REQUIRE(inner_count == 0);
}

// -----------------------------------------------------------------------------
// Edge: Single Iteration Inner Loops
// -----------------------------------------------------------------------------

TEST_CASE("Single iteration inner loops", "[nesting][edge]") {
    int count = 0;

    ILP_FOR_SIMPLE(i, 0, 10, 4) {
        ILP_FOR_SIMPLE(j, 0, 1, 4) {  // Only j=0
            ILP_FOR_SIMPLE(k, 0, 1, 4) {  // Only k=0
                count++;
            } ILP_END;
        } ILP_END;
    } ILP_END;

    REQUIRE(count == 10);
}

// -----------------------------------------------------------------------------
// Triangular Iteration Pattern
// -----------------------------------------------------------------------------

TEST_CASE("Triangular nested iteration", "[nesting][pattern]") {
    int count = 0;

    ILP_FOR_SIMPLE(i, 0, 10, 4) {
        ILP_FOR_SIMPLE(j, 0, i, 4) {  // j depends on i!
            count++;
        } ILP_END;
    } ILP_END;

    // 0 + 1 + 2 + ... + 9 = 45
    REQUIRE(count == 45);
}

TEST_CASE("Triangular with accumulation", "[nesting][pattern]") {
    int sum = 0;

    ILP_FOR_SIMPLE(i, 0, 10, 4) {
        ILP_FOR_SIMPLE(j, 0, i, 4) {
            sum += i + j;
        } ILP_END;
    } ILP_END;

    // Calculate expected
    int expected = 0;
    for (int i = 0; i < 10; ++i)
        for (int j = 0; j < i; ++j)
            expected += i + j;

    REQUIRE(sum == expected);
}
