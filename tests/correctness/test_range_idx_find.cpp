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

// ILP find using bool mode (returns index)
std::size_t ilp_find_bool(std::span<const int> arr, int target) {
    return ILP_FOR_RET_SIMPLE(i, 0uz, arr.size(), 4) {
        return arr[i] == target;  // bool mode
    } ILP_END;
}

// ILP find using optional mode (returns value)
std::optional<int> ilp_find_optional(std::span<const int> arr, int target) {
    return ILP_FOR_RET_SIMPLE(i, 0uz, arr.size(), 4) -> std::optional<int> {
        if (arr[i] == target) return arr[i] * 2;  // return computed value
        return std::nullopt;
    } ILP_END;
}

// ILP range-based find using bool mode (returns iterator)
auto ilp_range_find_bool(std::span<const int> arr, int target) {
    return ILP_FOR_RANGE_IDX_RET_SIMPLE(val, idx, arr, 4) {
        return val == target;  // bool mode - returns iterator
    } ILP_END;
}

TEST_CASE("Bool mode find (returns index)", "[find][bool]") {
    std::vector<int> data = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23};
    std::span<const int> arr(data);

    SECTION("find in middle") {
        auto idx = ilp_find_bool(arr, 11);
        REQUIRE(idx == 5);
    }

    SECTION("find first element") {
        auto idx = ilp_find_bool(arr, 1);
        REQUIRE(idx == 0);
    }

    SECTION("find last element") {
        auto idx = ilp_find_bool(arr, 23);
        REQUIRE(idx == 11);
    }

    SECTION("element not found") {
        auto idx = ilp_find_bool(arr, 100);
        REQUIRE(idx == arr.size());  // returns end sentinel
    }
}

TEST_CASE("Optional mode find (returns value)", "[find][optional]") {
    std::vector<int> data = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23};
    std::span<const int> arr(data);

    SECTION("find returns computed value") {
        auto result = ilp_find_optional(arr, 11);
        REQUIRE(result.has_value());
        REQUIRE(*result == 22);  // 11 * 2
    }

    SECTION("not found returns nullopt") {
        auto result = ilp_find_optional(arr, 100);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("Range bool mode find (returns iterator)", "[find][range][bool]") {
    std::vector<int> data = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23};
    std::span<const int> arr(data);

    SECTION("find returns iterator") {
        auto it = ilp_range_find_bool(arr, 11);
        REQUIRE(it != arr.end());
        REQUIRE(*it == 11);
        REQUIRE(it - arr.begin() == 5);
    }

    SECTION("not found returns end iterator") {
        auto it = ilp_range_find_bool(arr, 100);
        REQUIRE(it == arr.end());
    }
}

TEST_CASE("Find edge cases", "[find][edge]") {
    SECTION("empty array - bool mode") {
        std::vector<int> empty;
        std::span<const int> empty_arr(empty);

        auto idx = ilp_find_bool(empty_arr, 1);
        REQUIRE(idx == 0);  // end sentinel
    }

    SECTION("empty array - optional mode") {
        std::vector<int> empty;
        std::span<const int> empty_arr(empty);

        auto result = ilp_find_optional(empty_arr, 1);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("size not divisible by unroll factor") {
        std::vector<int> odd_data = {1, 2, 3, 4, 5, 6, 7};
        std::span<const int> odd_arr(odd_data);

        auto idx = ilp_find_bool(odd_arr, 7);
        REQUIRE(idx == 6);
    }

    SECTION("single element found") {
        std::vector<int> single = {42};
        std::span<const int> single_arr(single);

        auto idx = ilp_find_bool(single_arr, 42);
        REQUIRE(idx == 0);
    }

    SECTION("single element not found") {
        std::vector<int> single = {42};
        std::span<const int> single_arr(single);

        auto idx = ilp_find_bool(single_arr, 99);
        REQUIRE(idx == 1);  // end sentinel
    }

    SECTION("matches handrolled implementation") {
        std::vector<int> data = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23};
        std::span<const int> arr(data);

        for (int target : {1, 7, 11, 23, 100}) {
            auto hand_result = handrolled_find(arr, target);
            auto bool_result = ilp_find_bool(arr, target);

            if (hand_result.has_value()) {
                REQUIRE(bool_result == *hand_result);
            } else {
                REQUIRE(bool_result == arr.size());
            }
        }
    }
}
