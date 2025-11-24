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
