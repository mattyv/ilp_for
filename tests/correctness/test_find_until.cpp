// Tests for find_auto and find_range_idx_auto API functions

#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <array>
#include <vector>

#if !defined(ILP_MODE_SIMPLE)

// ============================================================================
// ilp::find_auto tests
// ============================================================================

TEST_CASE("ilp::find_auto basic functionality", "[find_auto][basic]") {
    std::vector<int> data = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23};

    SECTION("find element in middle") {
        auto idx = ilp::find_auto(0, (int)data.size(), [&](auto i, auto) { return data[i] == 11; });
        REQUIRE(idx == 5);
    }

    SECTION("find first element") {
        auto idx = ilp::find_auto(0, (int)data.size(), [&](auto i, auto) { return data[i] == 1; });
        REQUIRE(idx == 0);
    }

    SECTION("find last element") {
        auto idx = ilp::find_auto(0, (int)data.size(), [&](auto i, auto) { return data[i] == 23; });
        REQUIRE(idx == 11);
    }

    SECTION("element not found returns end") {
        auto idx = ilp::find_auto(0, (int)data.size(), [&](auto i, auto) { return data[i] == 100; });
        REQUIRE(idx == (int)data.size());
    }
}

TEST_CASE("ilp::find_auto edge cases", "[find_auto][edge]") {
    SECTION("empty range") {
        auto idx = ilp::find_auto(0, 0, [](auto, auto) { return true; });
        REQUIRE(idx == 0);
    }

    SECTION("single element - found") {
        std::vector<int> data = {42};
        auto idx = ilp::find_auto(0, 1, [&](auto i, auto) { return data[i] == 42; });
        REQUIRE(idx == 0);
    }

    SECTION("single element - not found") {
        std::vector<int> data = {42};
        auto idx = ilp::find_auto(0, 1, [&](auto i, auto) { return data[i] == 99; });
        REQUIRE(idx == 1);
    }

    SECTION("size_t indices") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8};
        auto idx = ilp::find_auto(0uz, data.size(), [&](auto i, auto) { return data[i] == 5; });
        REQUIRE(idx == 4uz);
    }
}

TEST_CASE("ilp::find_auto cleanup loop coverage", "[find_auto][cleanup]") {
    // Different sizes to exercise cleanup loops
    for (int size = 1; size <= 10; ++size) {
        std::vector<int> data(size);
        for (int i = 0; i < size; ++i)
            data[i] = i;

        SECTION("size = " + std::to_string(size) + " find last") {
            auto idx = ilp::find_auto(0, size, [&](auto i, auto) { return data[i] == size - 1; });
            REQUIRE(idx == size - 1);
        }
    }
}

TEST_CASE("ilp::find_auto with complex predicate", "[find_auto][predicate]") {
    std::vector<int> data = {1, 4, 9, 16, 25, 36, 49, 64};

    SECTION("find first value > 20") {
        auto idx = ilp::find_auto(0, (int)data.size(), [&](auto i, auto) { return data[i] > 20; });
        REQUIRE(idx == 4); // 25 at index 4
    }

    SECTION("find first even index with value > 10") {
        auto idx = ilp::find_auto(0, (int)data.size(), [&](auto i, auto) { return (i % 2 == 0) && data[i] > 10; });
        REQUIRE(idx == 4); // index 4, value 25
    }
}

// ============================================================================
// ilp::find_range_idx_auto tests
// ============================================================================

TEST_CASE("ilp::find_range_idx_auto basic functionality", "[find_range_idx_auto][basic]") {
    std::vector<int> data = {1, 3, 5, 7, 9, 11, 13, 15};

    SECTION("finds element and returns iterator") {
        auto it = ilp::find_range_idx_auto(data, [](auto&& val, auto idx, auto end) {
            (void)idx;
            (void)end;
            return val == 9;
        });
        REQUIRE(it != data.end());
        REQUIRE(*it == 9);
    }

    SECTION("not found returns end") {
        auto it = ilp::find_range_idx_auto(data, [](auto&& val, auto idx, auto end) {
            (void)idx;
            (void)end;
            return val == 100;
        });
        REQUIRE(it == data.end());
    }

    SECTION("can use index in predicate") {
        auto it = ilp::find_range_idx_auto(data, [](auto&&, auto idx, auto) { return idx == 3; });
        REQUIRE(it != data.end());
        REQUIRE(*it == 7); // data[3] == 7
    }
}

TEST_CASE("ilp::find_range_idx_auto edge cases", "[find_range_idx_auto][edge]") {
    SECTION("empty range") {
        std::vector<int> empty;
        auto it = ilp::find_range_idx_auto(empty, [](auto&&, auto, auto) { return true; });
        REQUIRE(it == empty.end());
    }

    SECTION("single element - found") {
        std::vector<int> single = {42};
        auto it = ilp::find_range_idx_auto(single, [](auto&& val, auto, auto) { return val == 42; });
        REQUIRE(it != single.end());
        REQUIRE(*it == 42);
    }

    SECTION("single element - not found") {
        std::vector<int> single = {42};
        auto it = ilp::find_range_idx_auto(single, [](auto&& val, auto, auto) { return val == 99; });
        REQUIRE(it == single.end());
    }
}

TEST_CASE("ilp::find_range_idx_auto with different containers", "[find_range_idx_auto][containers]") {
    SECTION("std::vector") {
        std::vector<int> vec = {1, 2, 3, 4, 5};
        auto it = ilp::find_range_idx_auto(vec, [](auto&& val, auto, auto) { return val == 3; });
        REQUIRE(it != vec.end());
        REQUIRE(*it == 3);
    }

    SECTION("std::array") {
        std::array<int, 5> arr = {1, 2, 3, 4, 5};
        auto it = ilp::find_range_idx_auto(arr, [](auto&& val, auto, auto) { return val == 3; });
        REQUIRE(it != arr.end());
        REQUIRE(*it == 3);
    }
}

#endif // !ILP_MODE_SIMPLE
