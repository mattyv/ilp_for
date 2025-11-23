#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <numeric>
#include <algorithm>
#include <climits>

TEST_CASE("Auto-selecting reduce_sum_auto", "[auto][reduce]") {
    std::vector<int> data(1000);
    std::iota(data.begin(), data.end(), 1);
    int expected = std::accumulate(data.begin(), data.end(), 0);

    SECTION("Index-based sum") {
        int sum = ilp::reduce_sum_auto(0, (int)data.size(), [&](int i) {
            return data[i];
        });
        REQUIRE(sum == expected);
    }

    SECTION("Macro version") {
        int sum = ILP_REDUCE_SUM_AUTO(i, 0, (int)data.size()) {
            return data[i];
        } ILP_END_REDUCE;
        REQUIRE(sum == expected);
    }
}

TEST_CASE("Auto-selecting reduce_range_sum_auto", "[auto][reduce][range]") {
    std::vector<int> data(1000);
    std::iota(data.begin(), data.end(), 1);
    int expected = std::accumulate(data.begin(), data.end(), 0);

    SECTION("Function version") {
        int sum = ilp::reduce_range_sum_auto(data, [](int val) {
            return val;
        });
        REQUIRE(sum == expected);
    }

    SECTION("Macro version") {
        int sum = ILP_REDUCE_RANGE_SUM_AUTO(val, data) {
            return val;
        } ILP_END_REDUCE;
        REQUIRE(sum == expected);
    }
}

TEST_CASE("Auto-selecting reduce_simple_auto for min", "[auto][reduce][min]") {
    std::vector<int> data = {5, 3, 8, 1, 9, 2, 7, 4, 6};
    int expected = *std::min_element(data.begin(), data.end());

    SECTION("Function version") {
        int min_val = ilp::reduce_simple_auto(0, (int)data.size(), INT_MAX,
            [](int a, int b) { return std::min(a, b); },
            [&](int i) { return data[i]; });
        REQUIRE(min_val == expected);
    }

    SECTION("Macro version") {
        int min_val = ILP_REDUCE_SIMPLE_AUTO(
            [](int a, int b) { return std::min(a, b); },
            INT_MAX, i, 0, (int)data.size()
        ) {
            return data[i];
        } ILP_END_REDUCE;
        REQUIRE(min_val == expected);
    }
}

TEST_CASE("Auto-selecting reduce_range_simple_auto", "[auto][reduce][range]") {
    std::vector<int> data = {5, 3, 8, 1, 9, 2, 7, 4, 6};

    SECTION("Min") {
        int expected = *std::min_element(data.begin(), data.end());
        int min_val = ILP_REDUCE_RANGE_SIMPLE_AUTO(
            [](int a, int b) { return std::min(a, b); },
            INT_MAX, val, data
        ) {
            return val;
        } ILP_END_REDUCE;
        REQUIRE(min_val == expected);
    }

    SECTION("Max") {
        int expected = *std::max_element(data.begin(), data.end());
        int max_val = ILP_REDUCE_RANGE_SIMPLE_AUTO(
            [](int a, int b) { return std::max(a, b); },
            INT_MIN, val, data
        ) {
            return val;
        } ILP_END_REDUCE;
        REQUIRE(max_val == expected);
    }

    SECTION("Count") {
        int expected = std::count_if(data.begin(), data.end(), [](int x) { return x > 5; });
        int count = ILP_REDUCE_RANGE_SIMPLE_AUTO(std::plus<>{}, 0, val, data) {
            return (val > 5) ? 1 : 0;
        } ILP_END_REDUCE;
        REQUIRE(count == expected);
    }
}

TEST_CASE("Different element sizes use different N", "[auto][optimal_N]") {
    // Just verify these compile - optimal_N selection happens at compile time

    SECTION("int8") {
        std::vector<int8_t> data(100, 1);
        int8_t sum = ilp::reduce_range_sum_auto(data, [](int8_t x) { return x; });
        REQUIRE(sum == 100);
    }

    SECTION("int16") {
        std::vector<int16_t> data(100, 1);
        int16_t sum = ilp::reduce_range_sum_auto(data, [](int16_t x) { return x; });
        REQUIRE(sum == 100);
    }

    SECTION("int32") {
        std::vector<int32_t> data(100, 1);
        int32_t sum = ilp::reduce_range_sum_auto(data, [](int32_t x) { return x; });
        REQUIRE(sum == 100);
    }

    SECTION("int64") {
        std::vector<int64_t> data(100, 1);
        int64_t sum = ilp::reduce_range_sum_auto(data, [](int64_t x) { return x; });
        REQUIRE(sum == 100);
    }
}
