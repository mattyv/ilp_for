#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <numeric>
#include <algorithm>
#include <climits>
#include <span>

TEST_CASE("Auto-selecting reduce sum", "[auto][reduce]") {
    std::vector<int> data(1000);
    std::iota(data.begin(), data.end(), 1);
    int expected = std::accumulate(data.begin(), data.end(), 0);

    SECTION("Index-based sum") {
        int sum = ilp::reduce_auto(0, (int)data.size(), 0,
            std::plus<>{},
            [&](int i) { return data[i]; });
        REQUIRE(sum == expected);
    }

    SECTION("Macro version") {
        int sum = ILP_REDUCE_AUTO(std::plus<>{}, 0, auto i, 0, (int)data.size()) {
            return data[i];
        } ILP_END_REDUCE;
        REQUIRE(sum == expected);
    }
}

TEST_CASE("Auto-selecting reduce range sum", "[auto][reduce][range]") {
    std::vector<int> data(1000);
    std::iota(data.begin(), data.end(), 1);
    int expected = std::accumulate(data.begin(), data.end(), 0);

    SECTION("Function version") {
        int sum = ilp::reduce_range_auto(data, 0, std::plus<>{},
            [](int val) { return val; });
        REQUIRE(sum == expected);
    }

    SECTION("Macro version") {
        int sum = ILP_REDUCE_RANGE_AUTO(std::plus<>{}, 0, auto&& val, data) {
            return val;
        } ILP_END_REDUCE;
        REQUIRE(sum == expected);
    }
}

TEST_CASE("Auto-selecting reduce for min", "[auto][reduce][min]") {
    std::vector<int> data = {5, 3, 8, 1, 9, 2, 7, 4, 6};
    int expected = *std::min_element(data.begin(), data.end());

    SECTION("Function version") {
        int min_val = ilp::reduce_auto(0, (int)data.size(), INT_MAX,
            [](int a, int b) { return std::min(a, b); },
            [&](int i) { return data[i]; });
        REQUIRE(min_val == expected);
    }

    SECTION("Macro version") {
        int min_val = ILP_REDUCE_AUTO(
            [](int a, int b) { return std::min(a, b); },
            INT_MAX, auto i, 0, (int)data.size()
        ) {
            return data[i];
        } ILP_END_REDUCE;
        REQUIRE(min_val == expected);
    }
}

TEST_CASE("Auto-selecting reduce range", "[auto][reduce][range]") {
    std::vector<int> data = {5, 3, 8, 1, 9, 2, 7, 4, 6};

    SECTION("Min") {
        int expected = *std::min_element(data.begin(), data.end());
        int min_val = ILP_REDUCE_RANGE_AUTO(
            [](int a, int b) { return std::min(a, b); },
            INT_MAX, auto&& val, data
        ) {
            return val;
        } ILP_END_REDUCE;
        REQUIRE(min_val == expected);
    }

    SECTION("Max") {
        int expected = *std::max_element(data.begin(), data.end());
        int max_val = ILP_REDUCE_RANGE_AUTO(
            [](int a, int b) { return std::max(a, b); },
            INT_MIN, auto&& val, data
        ) {
            return val;
        } ILP_END_REDUCE;
        REQUIRE(max_val == expected);
    }

    SECTION("Count") {
        int expected = std::count_if(data.begin(), data.end(), [](int x) { return x > 5; });
        int count = ILP_REDUCE_RANGE_AUTO(std::plus<>{}, 0, auto&& val, data) {
            return (val > 5) ? 1 : 0;
        } ILP_END_REDUCE;
        REQUIRE(count == expected);
    }
}

// Tests for transform_reduce dispatch (no-ctrl path)
// When lambda doesn't take ctrl, we dispatch to std::transform_reduce for SIMD
TEST_CASE("Range reduce without ctrl uses transform_reduce", "[reduce][range][transform_reduce]") {
    std::vector<int> data(1000);
    std::iota(data.begin(), data.end(), 1);

    SECTION("Sum with std::plus") {
        int expected = std::accumulate(data.begin(), data.end(), 0);
        // No ctrl in lambda -> transform_reduce path
        int sum = ilp::reduce_range<4>(data, 0, std::plus<>{},
            [](int val) { return val; });
        REQUIRE(sum == expected);
    }

    SECTION("Sum with auto N selection") {
        int expected = std::accumulate(data.begin(), data.end(), 0);
        int sum = ilp::reduce_range_auto(data, 0, std::plus<>{},
            [](int val) { return val; });
        REQUIRE(sum == expected);
    }

    SECTION("Product") {
        std::vector<int> small_data = {1, 2, 3, 4, 5};
        int expected = 1 * 2 * 3 * 4 * 5;
        int product = ilp::reduce_range<4>(small_data, 1, std::multiplies<>{},
            [](int val) { return val; });
        REQUIRE(product == expected);
    }

    SECTION("Min with custom op") {
        int expected = *std::min_element(data.begin(), data.end());
        int min_val = ilp::reduce_range<4>(data, INT_MAX,
            [](int a, int b) { return std::min(a, b); },
            [](int val) { return val; });
        REQUIRE(min_val == expected);
    }

    SECTION("Max with custom op") {
        int expected = *std::max_element(data.begin(), data.end());
        int max_val = ilp::reduce_range<4>(data, INT_MIN,
            [](int a, int b) { return std::max(a, b); },
            [](int val) { return val; });
        REQUIRE(max_val == expected);
    }

    SECTION("Transform and reduce") {
        // Sum of squares
        int expected = 0;
        for (int v : data) expected += v * v;

        int sum_sq = ilp::reduce_range<4>(data, 0, std::plus<>{},
            [](int val) { return val * val; });
        REQUIRE(sum_sq == expected);
    }

    SECTION("Count elements matching predicate") {
        int expected = std::count_if(data.begin(), data.end(), [](int x) { return x % 2 == 0; });
        int count = ilp::reduce_range<4>(data, 0, std::plus<>{},
            [](int val) { return (val % 2 == 0) ? 1 : 0; });
        REQUIRE(count == expected);
    }

    SECTION("Different container types") {
        // std::span
        std::span<int> span_data(data);
        int expected = std::accumulate(data.begin(), data.end(), 0);
        int sum = ilp::reduce_range<4>(span_data, 0, std::plus<>{},
            [](int val) { return val; });
        REQUIRE(sum == expected);
    }
}

TEST_CASE("Different element sizes use different N", "[auto][optimal_N]") {
    // Just verify these compile - optimal_N selection happens at compile time

    SECTION("int8") {
        std::vector<int8_t> data(100, 1);
        int8_t sum = ilp::reduce_range_auto(data, int8_t{0}, std::plus<>{},
            [](int8_t x) { return x; });
        REQUIRE(sum == 100);
    }

    SECTION("int16") {
        std::vector<int16_t> data(100, 1);
        int16_t sum = ilp::reduce_range_auto(data, int16_t{0}, std::plus<>{},
            [](int16_t x) { return x; });
        REQUIRE(sum == 100);
    }

    SECTION("int32") {
        std::vector<int32_t> data(100, 1);
        int32_t sum = ilp::reduce_range_auto(data, int32_t{0}, std::plus<>{},
            [](int32_t x) { return x; });
        REQUIRE(sum == 100);
    }

    SECTION("int64") {
        std::vector<int64_t> data(100, 1);
        int64_t sum = ilp::reduce_range_auto(data, int64_t{0}, std::plus<>{},
            [](int64_t x) { return x; });
        REQUIRE(sum == 100);
    }
}
