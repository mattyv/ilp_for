#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <span>
#include <optional>

// Hand-rolled declarations
int sum_range_handrolled(std::span<const int> data);
std::optional<std::size_t> find_value_handrolled(std::span<const int> data, int target);

// ILP declarations
int sum_range_ilp(std::span<const int> data);
std::optional<std::size_t> find_value_ilp(std::span<const int> data, int target);

TEST_CASE("Range-based sum", "[range]") {
    SECTION("typical values") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        REQUIRE(sum_range_ilp(data) == sum_range_handrolled(data));
    }

    SECTION("edge cases") {
        std::vector<int> empty = {};
        REQUIRE(sum_range_ilp(empty) == sum_range_handrolled(empty));

        std::vector<int> one = {42};
        REQUIRE(sum_range_ilp(one) == sum_range_handrolled(one));

        std::vector<int> three = {1, 2, 3};
        REQUIRE(sum_range_ilp(three) == sum_range_handrolled(three));

        std::vector<int> four = {1, 2, 3, 4};
        REQUIRE(sum_range_ilp(four) == sum_range_handrolled(four));

        std::vector<int> five = {1, 2, 3, 4, 5};
        REQUIRE(sum_range_ilp(five) == sum_range_handrolled(five));
    }

    SECTION("negative values") {
        std::vector<int> mixed = {-5, -3, 0, 2, 4, -1, 3};
        REQUIRE(sum_range_ilp(mixed) == sum_range_handrolled(mixed));
    }
}

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
}
