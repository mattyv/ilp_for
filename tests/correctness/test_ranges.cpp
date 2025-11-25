#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <span>
#include <optional>

// These tests compare against asm_compare implementations (Unix only)
#ifndef _MSC_VER

// Hand-rolled declarations
unsigned sum_range_handrolled(std::span<const unsigned> data);
std::optional<std::size_t> find_value_handrolled(std::span<const int> data, int target);
std::optional<std::size_t> find_value_no_ctrl_handrolled(std::span<const int> data, int target);

// ILP declarations
unsigned sum_range_ilp(std::span<const unsigned> data);
#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)
std::optional<std::size_t> find_value_ilp(std::span<const int> data, int target);
std::optional<std::size_t> find_value_no_ctrl_ilp(std::span<const int> data, int target);
#endif

TEST_CASE("Range-based sum", "[range]") {
    SECTION("typical values") {
        std::vector<unsigned> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        REQUIRE(sum_range_ilp(data) == sum_range_handrolled(data));
    }

    SECTION("edge cases") {
        std::vector<unsigned> empty = {};
        REQUIRE(sum_range_ilp(empty) == sum_range_handrolled(empty));

        std::vector<unsigned> one = {42};
        REQUIRE(sum_range_ilp(one) == sum_range_handrolled(one));

        std::vector<unsigned> three = {1, 2, 3};
        REQUIRE(sum_range_ilp(three) == sum_range_handrolled(three));

        std::vector<unsigned> four = {1, 2, 3, 4};
        REQUIRE(sum_range_ilp(four) == sum_range_handrolled(four));

        std::vector<unsigned> five = {1, 2, 3, 4, 5};
        REQUIRE(sum_range_ilp(five) == sum_range_handrolled(five));
    }

    SECTION("large values") {
        std::vector<unsigned> mixed = {100, 200, 0, 50, 150, 10, 300};
        REQUIRE(sum_range_ilp(mixed) == sum_range_handrolled(mixed));
    }

    SECTION("partial ranges via span") {
        std::vector<unsigned> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

        // Middle portion
        std::span<const unsigned> middle(data.data() + 2, 5);  // {3, 4, 5, 6, 7}
        REQUIRE(sum_range_ilp(middle) == sum_range_handrolled(middle));

        // Skip first elements
        std::span<const unsigned> skip_first(data.data() + 3, data.size() - 3);  // {4, 5, 6, 7, 8, 9, 10}
        REQUIRE(sum_range_ilp(skip_first) == sum_range_handrolled(skip_first));

        // Skip last elements
        std::span<const unsigned> skip_last(data.data(), data.size() - 3);  // {1, 2, 3, 4, 5, 6, 7}
        REQUIRE(sum_range_ilp(skip_last) == sum_range_handrolled(skip_last));

        // Single element subset
        std::span<const unsigned> single(data.data() + 4, 1);  // {5}
        REQUIRE(sum_range_ilp(single) == sum_range_handrolled(single));

        // Boundary cases (N-1, N, N+1 where N=4)
        std::span<const unsigned> three_elem(data.data() + 1, 3);  // {2, 3, 4}
        REQUIRE(sum_range_ilp(three_elem) == sum_range_handrolled(three_elem));

        std::span<const unsigned> four_elem(data.data() + 1, 4);  // {2, 3, 4, 5}
        REQUIRE(sum_range_ilp(four_elem) == sum_range_handrolled(four_elem));

        std::span<const unsigned> five_elem(data.data() + 1, 5);  // {2, 3, 4, 5, 6}
        REQUIRE(sum_range_ilp(five_elem) == sum_range_handrolled(five_elem));
    }
}

#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)
TEST_CASE("Find value (early return)", "[range][return]") {
    std::vector<int> data = {10, 20, 30, 40, 50, 60, 70, 80};

    SECTION("found at various positions") {
        REQUIRE(find_value_ilp(data, 10) == find_value_handrolled(data, 10));  // first
        REQUIRE(find_value_ilp(data, 30) == find_value_handrolled(data, 30));  // middle
        REQUIRE(find_value_ilp(data, 80) == find_value_handrolled(data, 80));  // last
        REQUIRE(find_value_ilp(data, 50) == find_value_handrolled(data, 50));  // boundary
    }

    SECTION("not found") {
        REQUIRE(find_value_ilp(data, 99) == find_value_handrolled(data, 99));
        REQUIRE(find_value_ilp(data, 0) == find_value_handrolled(data, 0));
    }

    SECTION("empty data") {
        std::vector<int> empty = {};
        REQUIRE(find_value_ilp(empty, 42) == find_value_handrolled(empty, 42));
    }

    SECTION("single element") {
        std::vector<int> one = {42};
        REQUIRE(find_value_ilp(one, 42) == find_value_handrolled(one, 42));
        REQUIRE(find_value_ilp(one, 0) == find_value_handrolled(one, 0));
    }

    SECTION("partial ranges via span") {
        std::vector<int> data = {10, 20, 30, 40, 50, 60, 70, 80};

        // Search in middle portion {30, 40, 50, 60}
        std::span<const int> middle(data.data() + 2, 4);
        REQUIRE(find_value_ilp(middle, 40) == find_value_handrolled(middle, 40));
        REQUIRE(find_value_ilp(middle, 10) == find_value_handrolled(middle, 10));  // not in range

        // Skip first elements {40, 50, 60, 70, 80}
        std::span<const int> skip_first(data.data() + 3, 5);
        REQUIRE(find_value_ilp(skip_first, 50) == find_value_handrolled(skip_first, 50));
        REQUIRE(find_value_ilp(skip_first, 80) == find_value_handrolled(skip_first, 80));  // last

        // Boundary cases
        std::span<const int> three_elem(data.data() + 1, 3);  // {20, 30, 40}
        REQUIRE(find_value_ilp(three_elem, 30) == find_value_handrolled(three_elem, 30));
        REQUIRE(find_value_ilp(three_elem, 99) == find_value_handrolled(three_elem, 99));  // not found
    }
}
#endif

#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)
TEST_CASE("Find value no ctrl (return-only, no control flow)", "[range][return]") {
    std::vector<int> data = {10, 20, 30, 40, 50, 60, 70, 80};

    SECTION("found at various positions") {
        REQUIRE(find_value_no_ctrl_ilp(data, 10) == find_value_no_ctrl_handrolled(data, 10));  // first
        REQUIRE(find_value_no_ctrl_ilp(data, 30) == find_value_no_ctrl_handrolled(data, 30));  // middle
        REQUIRE(find_value_no_ctrl_ilp(data, 80) == find_value_no_ctrl_handrolled(data, 80));  // last
        REQUIRE(find_value_no_ctrl_ilp(data, 50) == find_value_no_ctrl_handrolled(data, 50));  // boundary
    }

    SECTION("not found") {
        REQUIRE(find_value_no_ctrl_ilp(data, 99) == find_value_no_ctrl_handrolled(data, 99));
        REQUIRE(find_value_no_ctrl_ilp(data, 0) == find_value_no_ctrl_handrolled(data, 0));
    }

    SECTION("empty data") {
        std::vector<int> empty = {};
        REQUIRE(find_value_no_ctrl_ilp(empty, 42) == find_value_no_ctrl_handrolled(empty, 42));
    }

    SECTION("single element") {
        std::vector<int> one = {42};
        REQUIRE(find_value_no_ctrl_ilp(one, 42) == find_value_no_ctrl_handrolled(one, 42));
        REQUIRE(find_value_no_ctrl_ilp(one, 0) == find_value_no_ctrl_handrolled(one, 0));
    }

    SECTION("boundary cases around unroll factor") {
        // Test sizes n-1, n, n+1 where n=4
        std::vector<int> three = {1, 2, 3};
        REQUIRE(find_value_no_ctrl_ilp(three, 2) == find_value_no_ctrl_handrolled(three, 2));
        REQUIRE(find_value_no_ctrl_ilp(three, 99) == find_value_no_ctrl_handrolled(three, 99));

        std::vector<int> four = {1, 2, 3, 4};
        REQUIRE(find_value_no_ctrl_ilp(four, 1) == find_value_no_ctrl_handrolled(four, 1));
        REQUIRE(find_value_no_ctrl_ilp(four, 4) == find_value_no_ctrl_handrolled(four, 4));
        REQUIRE(find_value_no_ctrl_ilp(four, 99) == find_value_no_ctrl_handrolled(four, 99));

        std::vector<int> five = {1, 2, 3, 4, 5};
        REQUIRE(find_value_no_ctrl_ilp(five, 5) == find_value_no_ctrl_handrolled(five, 5));  // in tail
        REQUIRE(find_value_no_ctrl_ilp(five, 3) == find_value_no_ctrl_handrolled(five, 3));  // in main loop
    }

    SECTION("find in each position of unrolled block") {
        std::vector<int> data = {1, 2, 3, 4};
        for (int i = 1; i <= 4; ++i) {
            REQUIRE(find_value_no_ctrl_ilp(data, i) == find_value_no_ctrl_handrolled(data, i));
        }
    }
}
#endif

#endif // _MSC_VER
