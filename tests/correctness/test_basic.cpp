#include "catch.hpp"
#include "../../ilp_for.hpp"

// Hand-rolled declarations
unsigned sum_plain_handrolled(unsigned n);
unsigned sum_with_break_handrolled(unsigned n, unsigned stop_at);
unsigned sum_odd_handrolled(unsigned n);
int sum_negative_handrolled(int start, int end);
int sum_backward_handrolled(int start, int end);
unsigned sum_step2_handrolled(unsigned n);

// ILP declarations
unsigned sum_plain_ilp(unsigned n);
#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)
unsigned sum_with_break_ilp(unsigned n, unsigned stop_at);
#endif
unsigned sum_odd_ilp(unsigned n);
int sum_negative_ilp(int start, int end);
int sum_backward_ilp(int start, int end);
unsigned sum_step2_ilp(unsigned n);

// ============== Basic Tests ==============

TEST_CASE("Plain accumulation", "[basic]") {
    SECTION("typical values") {
        REQUIRE(sum_plain_ilp(100) == sum_plain_handrolled(100));
        REQUIRE(sum_plain_ilp(1000) == sum_plain_handrolled(1000));
    }

    SECTION("edge cases") {
        REQUIRE(sum_plain_ilp(0) == sum_plain_handrolled(0));
        REQUIRE(sum_plain_ilp(1) == sum_plain_handrolled(1));
        REQUIRE(sum_plain_ilp(3) == sum_plain_handrolled(3));  // n < N
        REQUIRE(sum_plain_ilp(4) == sum_plain_handrolled(4));  // n == N
        REQUIRE(sum_plain_ilp(5) == sum_plain_handrolled(5));  // n == N+1
    }
}

#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)
TEST_CASE("Break on condition", "[control]") {
    SECTION("breaks early") {
        REQUIRE(sum_with_break_ilp(100, 50) == sum_with_break_handrolled(100, 50));
        REQUIRE(sum_with_break_ilp(100, 10) == sum_with_break_handrolled(100, 10));
    }

    SECTION("no break needed") {
        REQUIRE(sum_with_break_ilp(10, 1000) == sum_with_break_handrolled(10, 1000));
    }

    SECTION("edge cases") {
        REQUIRE(sum_with_break_ilp(0, 50) == sum_with_break_handrolled(0, 50));
        REQUIRE(sum_with_break_ilp(1, 0) == sum_with_break_handrolled(1, 0));
    }
}
#endif

TEST_CASE("Continue (skip even)", "[control]") {
    SECTION("typical values") {
        REQUIRE(sum_odd_ilp(100) == sum_odd_handrolled(100));
        REQUIRE(sum_odd_ilp(101) == sum_odd_handrolled(101));
    }

    SECTION("edge cases") {
        REQUIRE(sum_odd_ilp(0) == sum_odd_handrolled(0));
        REQUIRE(sum_odd_ilp(1) == sum_odd_handrolled(1));
        REQUIRE(sum_odd_ilp(2) == sum_odd_handrolled(2));
    }
}

TEST_CASE("Negative range", "[signed]") {
    SECTION("crossing zero") {
        REQUIRE(sum_negative_ilp(-10, 10) == sum_negative_handrolled(-10, 10));
        REQUIRE(sum_negative_ilp(-5, 5) == sum_negative_handrolled(-5, 5));
    }

    SECTION("all negative") {
        REQUIRE(sum_negative_ilp(-20, -10) == sum_negative_handrolled(-20, -10));
    }

    SECTION("edge cases") {
        REQUIRE(sum_negative_ilp(0, 0) == sum_negative_handrolled(0, 0));
        REQUIRE(sum_negative_ilp(-1, 0) == sum_negative_handrolled(-1, 0));
    }
}

TEST_CASE("Backward iteration", "[step]") {
    SECTION("typical values") {
        REQUIRE(sum_backward_ilp(100, 0) == sum_backward_handrolled(100, 0));
        REQUIRE(sum_backward_ilp(50, 10) == sum_backward_handrolled(50, 10));
    }

    SECTION("crossing zero") {
        REQUIRE(sum_backward_ilp(10, -10) == sum_backward_handrolled(10, -10));
    }

    SECTION("edge cases") {
        REQUIRE(sum_backward_ilp(0, 0) == sum_backward_handrolled(0, 0));
        REQUIRE(sum_backward_ilp(3, 0) == sum_backward_handrolled(3, 0));
        REQUIRE(sum_backward_ilp(4, 0) == sum_backward_handrolled(4, 0));
    }
}

TEST_CASE("Step of 2", "[step]") {
    SECTION("typical values") {
        REQUIRE(sum_step2_ilp(100) == sum_step2_handrolled(100));
        REQUIRE(sum_step2_ilp(101) == sum_step2_handrolled(101));
    }

    SECTION("edge cases") {
        REQUIRE(sum_step2_ilp(0) == sum_step2_handrolled(0));
        REQUIRE(sum_step2_ilp(1) == sum_step2_handrolled(1));
        REQUIRE(sum_step2_ilp(7) == sum_step2_handrolled(7));
        REQUIRE(sum_step2_ilp(8) == sum_step2_handrolled(8));
    }
}

TEST_CASE("FOR loops with remainders hit cleanup", "[cleanup][for]") {
    SECTION("FOR_RET_SIMPLE with optional and remainder") {
        // 7 elements with unroll 4: hits cleanup for last 3 elements
        auto result = ILP_FOR_RET_SIMPLE(i, 0, 7, 4) -> std::optional<int> {
            if (i == 6) return std::optional<int>(i);  // Last element in cleanup
            return std::nullopt;
        } ILP_END;

        REQUIRE(result.has_value());
        REQUIRE(result.value() == 6);
    }

    SECTION("FOR_STEP_RET_SIMPLE with remainder - found") {
        // Step 2 from 0 to 11: visits 0,2,4,6,8,10 (6 values with unroll 4)
        auto result = ILP_FOR_STEP_RET_SIMPLE(i, 0, 11, 2, 4) {
            if (i == 10) return i;  // Last in cleanup
            return _ilp_end_;
        } ILP_END;

        REQUIRE(result == 10);
    }

    SECTION("FOR_STEP_RET_SIMPLE with remainder - not found") {
        // Test line 320: nothing found, returns end
        auto result = ILP_FOR_STEP_RET_SIMPLE(i, 0, 11, 2, 4) {
            if (i == 999) return i;  // Never found
            return _ilp_end_;
        } ILP_END;

        REQUIRE(result == 11);  // Returns end value
    }

    SECTION("FOR_RANGE_IDX_RET_SIMPLE with bool and remainder - found") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9};  // 9 elements

        auto it = ILP_FOR_RANGE_IDX_RET_SIMPLE(val, idx, data, 4) {
            return idx == 8;  // Last index in cleanup
        } ILP_END;

        REQUIRE(it != std::ranges::end(data));
        REQUIRE(*it == 9);
    }

    SECTION("FOR_RANGE_IDX_RET_SIMPLE with bool - not found") {
        // Test line 480: cleanup loop when nothing found
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9};

        auto it = ILP_FOR_RANGE_IDX_RET_SIMPLE(val, idx, data, 4) {
            return idx == 999;  // Never found
        } ILP_END;

        REQUIRE(it == std::ranges::end(data));
    }

    SECTION("FOR_RANGE_RET_SIMPLE with iterator - not found") {
        // Test lines 446-450: cleanup for range sentinel pattern
        std::vector<int> data = {10, 20, 30, 40, 50, 60, 70};  // 7 elements

        auto it = ILP_FOR_RANGE_RET_SIMPLE(val, data, 4) {
            if (val == 999) {  // Never matches
                return std::ranges::begin(data);
            }
            return _ilp_end_;
        } ILP_END;

        REQUIRE(it == std::ranges::end(data));
    }
}

