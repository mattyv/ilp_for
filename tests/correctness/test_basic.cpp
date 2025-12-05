#include "catch.hpp"
#include "../../ilp_for.hpp"

// ============== ASM Compare Tests (Unix only - requires asm_compare libs) ==============
#if !defined(_MSC_VER) && !defined(ILP_MODE_SUPER_SIMPLE)

// Hand-rolled declarations
unsigned sum_plain_handrolled(unsigned n);
unsigned sum_with_break_handrolled(unsigned n, unsigned stop_at);
unsigned sum_odd_handrolled(unsigned n);
int sum_negative_handrolled(int start, int end);

// ILP declarations
unsigned sum_plain_ilp(unsigned n);
#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA) && !defined(ILP_MODE_SUPER_SIMPLE)
unsigned sum_with_break_ilp(unsigned n, unsigned stop_at);
#endif
unsigned sum_odd_ilp(unsigned n);
int sum_negative_ilp(int start, int end);

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

#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA) && !defined(ILP_MODE_SUPER_SIMPLE)
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

#endif // !_MSC_VER && !ILP_MODE_SUPER_SIMPLE

#if !defined(ILP_MODE_SUPER_SIMPLE)
TEST_CASE("FOR loops with remainders hit cleanup", "[cleanup][for]") {
    SECTION("find with optional and remainder") {
        // 7 elements with unroll 4: hits cleanup for last 3 elements
        auto result = ilp::find<4>(0, 7, [](auto i, auto) -> std::optional<int> {
            if (i == 6) return std::optional<int>(i);  // Last element in cleanup
            return std::nullopt;
        });

        REQUIRE(result.has_value());
        REQUIRE(result.value() == 6);
    }

    SECTION("find_range_idx with bool and remainder - found") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9};  // 9 elements

        auto it = ilp::find_range_idx<4>(data, [](auto&& val, auto idx, auto) {
            return idx == 8;  // Last index in cleanup
        });

        REQUIRE(it != std::ranges::end(data));
        REQUIRE(*it == 9);
    }

    SECTION("find_range_idx with bool - not found") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9};

        auto it = ilp::find_range_idx<4>(data, [](auto&& val, auto idx, auto) {
            return idx == 999;  // Never found
        });

        REQUIRE(it == std::ranges::end(data));
    }

}
#endif // !ILP_MODE_SUPER_SIMPLE

