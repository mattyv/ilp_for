#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <span>
#include <optional>

// Hand-rolled find with index
std::optional<std::size_t> handrolled_find(std::span<const int> arr, int target) {
    const int* ptr = arr.data();
    std::size_t n = arr.size();

    // Unrolled by 4
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        if (ptr[i] == target) return i;
        if (ptr[i + 1] == target) return i + 1;
        if (ptr[i + 2] == target) return i + 2;
        if (ptr[i + 3] == target) return i + 3;
    }

    // Remainder
    for (; i < n; ++i) {
        if (ptr[i] == target) return i;
    }

    return std::nullopt;
}

// ILP find using new indexed function
std::optional<std::size_t> ilp_find(std::span<const int> arr, int target) {
    return ilp::for_loop_range_idx_ret_simple<std::size_t, 4>(arr,
        [&](auto val, auto idx) -> std::optional<std::size_t> {
            if (val == target) return idx;
            return std::nullopt;
        });
}

TEST_CASE("Range find with index", "[range][find]") {
    std::vector<int> data = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23};
    std::span<const int> arr(data);

    SECTION("find in middle") {
        auto ilp_result = ilp_find(arr, 11);
        auto hand_result = handrolled_find(arr, 11);

        REQUIRE(ilp_result.has_value());
        REQUIRE(hand_result.has_value());
        REQUIRE(*ilp_result == *hand_result);
        REQUIRE(*ilp_result == 5);
    }

    SECTION("find first element") {
        auto ilp_result = ilp_find(arr, 1);
        auto hand_result = handrolled_find(arr, 1);

        REQUIRE(ilp_result.has_value());
        REQUIRE(*ilp_result == 0);
        REQUIRE(*ilp_result == *hand_result);
    }

    SECTION("find last element") {
        auto ilp_result = ilp_find(arr, 23);
        auto hand_result = handrolled_find(arr, 23);

        REQUIRE(ilp_result.has_value());
        REQUIRE(*ilp_result == 11);
        REQUIRE(*ilp_result == *hand_result);
    }

    SECTION("element not found") {
        auto ilp_result = ilp_find(arr, 100);
        auto hand_result = handrolled_find(arr, 100);

        REQUIRE_FALSE(ilp_result.has_value());
        REQUIRE_FALSE(hand_result.has_value());
    }
}

TEST_CASE("Range find edge cases", "[range][find][edge]") {
    SECTION("empty array") {
        std::vector<int> empty;
        std::span<const int> empty_arr(empty);

        auto ilp_result = ilp_find(empty_arr, 1);

        REQUIRE_FALSE(ilp_result.has_value());
    }

    SECTION("size not divisible by unroll factor") {
        std::vector<int> odd_data = {1, 2, 3, 4, 5, 6, 7};
        std::span<const int> odd_arr(odd_data);

        auto ilp_result = ilp_find(odd_arr, 7);
        auto hand_result = handrolled_find(odd_arr, 7);

        REQUIRE(ilp_result.has_value());
        REQUIRE(*ilp_result == 6);
        REQUIRE(*ilp_result == *hand_result);
    }

    SECTION("single element found") {
        std::vector<int> single = {42};
        std::span<const int> single_arr(single);

        auto ilp_result = ilp_find(single_arr, 42);

        REQUIRE(ilp_result.has_value());
        REQUIRE(*ilp_result == 0);
    }

    SECTION("single element not found") {
        std::vector<int> single = {42};
        std::span<const int> single_arr(single);

        auto ilp_result = ilp_find(single_arr, 99);

        REQUIRE_FALSE(ilp_result.has_value());
    }
}
