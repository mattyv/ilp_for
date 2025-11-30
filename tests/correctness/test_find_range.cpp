#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <span>
#include <array>

// Helper function using ILP_FIND_RANGE - returns iterator
auto ilp_find_range_test(std::span<const int> arr, int target) {
    return ILP_FIND_RANGE(auto&& val, arr, 4) {
        return val == target;
    } ILP_END;
}

// Helper function using ILP_FIND_RANGE_AUTO - returns iterator
auto ilp_find_range_auto_test(std::span<const int> arr, int target) {
    return ILP_FIND_RANGE_AUTO(auto&& val, arr) {
        return val == target;
    } ILP_END;
}

// Helper to convert iterator result to index for testing
size_t to_index(std::span<const int> arr, auto it) {
    return static_cast<size_t>(it - arr.begin());
}

TEST_CASE("ILP_FIND_RANGE basic functionality", "[find_range][basic]") {
    std::vector<int> data = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23};
    std::span<const int> arr(data);

    SECTION("find element in middle") {
        auto it = ilp_find_range_test(arr, 11);
        REQUIRE(it != arr.end());
        REQUIRE(*it == 11);
        REQUIRE(to_index(arr, it) == 5);
    }

    SECTION("find first element") {
        auto it = ilp_find_range_test(arr, 1);
        REQUIRE(it != arr.end());
        REQUIRE(*it == 1);
        REQUIRE(to_index(arr, it) == 0);
    }

    SECTION("find last element") {
        auto it = ilp_find_range_test(arr, 23);
        REQUIRE(it != arr.end());
        REQUIRE(*it == 23);
        REQUIRE(to_index(arr, it) == 11);
    }

    SECTION("element not found returns end()") {
        auto it = ilp_find_range_test(arr, 100);
        REQUIRE(it == arr.end());
    }
}

TEST_CASE("ILP_FIND_RANGE edge cases", "[find_range][edge]") {
    SECTION("empty range") {
        std::vector<int> empty;
        std::span<const int> arr(empty);

        auto it = ilp_find_range_test(arr, 1);
        REQUIRE(it == arr.end());
    }

    SECTION("single element - found") {
        std::vector<int> single = {42};
        std::span<const int> arr(single);

        auto it = ilp_find_range_test(arr, 42);
        REQUIRE(it != arr.end());
        REQUIRE(*it == 42);
    }

    SECTION("single element - not found") {
        std::vector<int> single = {42};
        std::span<const int> arr(single);

        auto it = ilp_find_range_test(arr, 99);
        REQUIRE(it == arr.end());
    }

    SECTION("two elements - find first") {
        std::vector<int> two = {10, 20};
        std::span<const int> arr(two);

        auto it = ilp_find_range_test(arr, 10);
        REQUIRE(it != arr.end());
        REQUIRE(*it == 10);
    }

    SECTION("two elements - find second") {
        std::vector<int> two = {10, 20};
        std::span<const int> arr(two);

        auto it = ilp_find_range_test(arr, 20);
        REQUIRE(it != arr.end());
        REQUIRE(*it == 20);
    }
}

TEST_CASE("ILP_FIND_RANGE cleanup loop (size not divisible by N)", "[find_range][cleanup]") {
    SECTION("size = 5 (5 mod 4 = 1) - find in cleanup") {
        std::vector<int> data = {1, 2, 3, 4, 5};
        std::span<const int> arr(data);

        auto it = ilp_find_range_test(arr, 5);
        REQUIRE(it != arr.end());
        REQUIRE(*it == 5);
        REQUIRE(to_index(arr, it) == 4);  // Found in cleanup loop
    }

    SECTION("size = 6 (6 mod 4 = 2) - find in cleanup") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6};
        std::span<const int> arr(data);

        auto it = ilp_find_range_test(arr, 6);
        REQUIRE(it != arr.end());
        REQUIRE(*it == 6);
        REQUIRE(to_index(arr, it) == 5);  // Found in cleanup loop
    }

    SECTION("size = 7 (7 mod 4 = 3) - find in cleanup") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7};
        std::span<const int> arr(data);

        auto it = ilp_find_range_test(arr, 7);
        REQUIRE(it != arr.end());
        REQUIRE(*it == 7);
        REQUIRE(to_index(arr, it) == 6);  // Found in cleanup loop
    }

    SECTION("size = 3 (all in cleanup, N=4)") {
        std::vector<int> data = {10, 20, 30};
        std::span<const int> arr(data);

        REQUIRE(*ilp_find_range_test(arr, 10) == 10);
        REQUIRE(*ilp_find_range_test(arr, 20) == 20);
        REQUIRE(*ilp_find_range_test(arr, 30) == 30);
        REQUIRE(ilp_find_range_test(arr, 99) == arr.end());  // Not found
    }
}

TEST_CASE("ILP_FIND_RANGE with different N values", "[find_range][unroll]") {
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
    std::span<const int> arr(data);

    SECTION("N = 2") {
        auto it = ILP_FIND_RANGE(auto&& val, arr, 2) {
            return val == 17;
        } ILP_END;
        REQUIRE(it != arr.end());
        REQUIRE(*it == 17);
    }

    SECTION("N = 4") {
        auto it = ILP_FIND_RANGE(auto&& val, arr, 4) {
            return val == 17;
        } ILP_END;
        REQUIRE(it != arr.end());
        REQUIRE(*it == 17);
    }

    SECTION("N = 8") {
        auto it = ILP_FIND_RANGE(auto&& val, arr, 8) {
            return val == 17;
        } ILP_END;
        REQUIRE(it != arr.end());
        REQUIRE(*it == 17);
    }
}

TEST_CASE("ILP_FIND_RANGE_AUTO selects reasonable N", "[find_range][auto]") {
    std::vector<int> data = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23};
    std::span<const int> arr(data);

    SECTION("finds middle element") {
        auto it = ilp_find_range_auto_test(arr, 11);
        REQUIRE(it != arr.end());
        REQUIRE(*it == 11);
    }

    SECTION("finds first element") {
        auto it = ilp_find_range_auto_test(arr, 1);
        REQUIRE(it != arr.end());
        REQUIRE(*it == 1);
    }

    SECTION("finds last element") {
        auto it = ilp_find_range_auto_test(arr, 23);
        REQUIRE(it != arr.end());
        REQUIRE(*it == 23);
    }

    SECTION("not found returns end") {
        auto it = ilp_find_range_auto_test(arr, 100);
        REQUIRE(it == arr.end());
    }
}

TEST_CASE("ILP_FIND_RANGE with different container types", "[find_range][containers]") {
    SECTION("std::vector") {
        std::vector<int> vec = {1, 2, 3, 4, 5};
        auto it = ILP_FIND_RANGE(auto&& val, vec, 4) {
            return val == 3;
        } ILP_END;
        REQUIRE(it != vec.end());
        REQUIRE(*it == 3);
    }

    SECTION("std::span") {
        std::vector<int> vec = {1, 2, 3, 4, 5};
        std::span<const int> sp(vec);
        auto it = ILP_FIND_RANGE(auto&& val, sp, 4) {
            return val == 3;
        } ILP_END;
        REQUIRE(it != sp.end());
        REQUIRE(*it == 3);
    }

    SECTION("std::array") {
        std::array<int, 5> arr = {1, 2, 3, 4, 5};
        auto it = ILP_FIND_RANGE(auto&& val, arr, 4) {
            return val == 3;
        } ILP_END;
        REQUIRE(it != arr.end());
        REQUIRE(*it == 3);
    }
}

TEST_CASE("ILP_FIND_RANGE finds first match when duplicates exist", "[find_range][duplicates]") {
    std::vector<int> data = {1, 5, 5, 5, 9};
    std::span<const int> arr(data);

    auto it = ilp_find_range_test(arr, 5);
    REQUIRE(it != arr.end());
    REQUIRE(*it == 5);
    REQUIRE(to_index(arr, it) == 1);  // First occurrence
}

TEST_CASE("ILP_FIND_RANGE with complex predicate", "[find_range][predicate]") {
    std::vector<int> data = {1, 4, 9, 16, 25, 36, 49, 64};  // Squares
    std::span<const int> arr(data);

    SECTION("find first value > 20") {
        auto it = ILP_FIND_RANGE(auto&& val, arr, 4) {
            return val > 20;
        } ILP_END;
        REQUIRE(it != arr.end());
        REQUIRE(*it == 25);  // 25 is first > 20
    }

    SECTION("find first even value") {
        auto it = ILP_FIND_RANGE(auto&& val, arr, 4) {
            return val % 2 == 0;
        } ILP_END;
        REQUIRE(it != arr.end());
        REQUIRE(*it == 4);  // 4 is first even
    }
}
