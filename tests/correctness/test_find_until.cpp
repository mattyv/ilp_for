// Tests for find, find_if, find_auto, find_if_auto, and find_range_idx_auto API functions

#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <array>
#include <vector>

#if !defined(ILP_MODE_SIMPLE)

// ============================================================================
// ilp::find (value-based) tests
// ============================================================================

TEST_CASE("ilp::find value-based basic functionality", "[find][value][basic]") {
    std::vector<int> data = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23};

    SECTION("find element in middle") {
        auto it = ilp::find(data, 11);
        REQUIRE(it != data.end());
        REQUIRE(*it == 11);
        REQUIRE(std::distance(data.begin(), it) == 5);
    }

    SECTION("find first element") {
        auto it = ilp::find(data, 1);
        REQUIRE(it != data.end());
        REQUIRE(*it == 1);
        REQUIRE(std::distance(data.begin(), it) == 0);
    }

    SECTION("find last element") {
        auto it = ilp::find(data, 23);
        REQUIRE(it != data.end());
        REQUIRE(*it == 23);
        REQUIRE(std::distance(data.begin(), it) == 11);
    }

    SECTION("element not found returns end") {
        auto it = ilp::find(data, 100);
        REQUIRE(it == data.end());
    }
}

TEST_CASE("ilp::find_auto value-based", "[find_auto][value]") {
    std::vector<int> data = {1, 3, 5, 7, 9, 11, 13, 15};

    SECTION("finds element with auto-selected N") {
        auto it = ilp::find_auto(data, 9);
        REQUIRE(it != data.end());
        REQUIRE(*it == 9);
    }

    SECTION("not found returns end") {
        auto it = ilp::find_auto(data, 100);
        REQUIRE(it == data.end());
    }
}

// ============================================================================
// ilp::find_if (predicate-based) tests
// ============================================================================

TEST_CASE("ilp::find_if basic functionality", "[find_if][basic]") {
    std::vector<int> data = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23};

    SECTION("find element in middle with predicate") {
        auto it = ilp::find_if(data, [](auto&& val) { return val == 11; });
        REQUIRE(it != data.end());
        REQUIRE(*it == 11);
        REQUIRE(std::distance(data.begin(), it) == 5);
    }

    SECTION("find first element") {
        auto it = ilp::find_if(data, [](auto&& val) { return val == 1; });
        REQUIRE(it != data.end());
        REQUIRE(std::distance(data.begin(), it) == 0);
    }

    SECTION("find last element") {
        auto it = ilp::find_if(data, [](auto&& val) { return val == 23; });
        REQUIRE(it != data.end());
        REQUIRE(std::distance(data.begin(), it) == 11);
    }

    SECTION("element not found returns end") {
        auto it = ilp::find_if(data, [](auto&& val) { return val == 100; });
        REQUIRE(it == data.end());
    }
}

TEST_CASE("ilp::find_if_auto with predicate", "[find_if_auto][basic]") {
    std::vector<int> data = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23};

    SECTION("find element in middle") {
        auto it = ilp::find_if_auto(data, [](auto&& val) { return val == 11; });
        REQUIRE(it != data.end());
        REQUIRE(*it == 11);
    }

    SECTION("element not found returns end") {
        auto it = ilp::find_if_auto(data, [](auto&& val) { return val == 100; });
        REQUIRE(it == data.end());
    }
}

TEST_CASE("ilp::find_if edge cases", "[find_if][edge]") {
    SECTION("empty range") {
        std::vector<int> empty;
        auto it = ilp::find_if(empty, [](auto&&) { return true; });
        REQUIRE(it == empty.end());
    }

    SECTION("single element - found") {
        std::vector<int> data = {42};
        auto it = ilp::find_if(data, [](auto&& val) { return val == 42; });
        REQUIRE(it != data.end());
        REQUIRE(*it == 42);
    }

    SECTION("single element - not found") {
        std::vector<int> data = {42};
        auto it = ilp::find_if(data, [](auto&& val) { return val == 99; });
        REQUIRE(it == data.end());
    }
}

TEST_CASE("ilp::find_if cleanup loop coverage", "[find_if][cleanup]") {
    // Different sizes to exercise cleanup loops
    int size = GENERATE(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    std::vector<int> data(size);
    for (int i = 0; i < size; ++i)
        data[i] = i;

    auto it = ilp::find_if(data, [size](auto&& val) { return val == size - 1; });
    REQUIRE(it != data.end());
    REQUIRE(*it == size - 1);
}

TEST_CASE("ilp::find_if with complex predicate", "[find_if][predicate]") {
    std::vector<int> data = {1, 4, 9, 16, 25, 36, 49, 64};

    SECTION("find first value > 20") {
        auto it = ilp::find_if(data, [](auto&& val) { return val > 20; });
        REQUIRE(it != data.end());
        REQUIRE(*it == 25);
    }
}

// ============================================================================
// ilp::find_range_idx_auto tests (advanced API - kept)
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
