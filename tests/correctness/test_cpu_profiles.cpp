#include "catch.hpp"
#include "ilp_for.hpp"
#include <vector>
#include <numeric>

using namespace ilp;

TEST_CASE("CPU profile optimal_N values", "[cpu]") {
    SECTION("Sum loop type") {
        // Verify we can access optimal_N at compile time
        constexpr auto n_int8 = optimal_N<LoopType::Sum, 1>;
        constexpr auto n_int16 = optimal_N<LoopType::Sum, 2>;
        constexpr auto n_int32 = optimal_N<LoopType::Sum, 4>;
        constexpr auto n_int64 = optimal_N<LoopType::Sum, 8>;

        // Smaller types should have >= unrolling than larger types
        REQUIRE(n_int8 >= n_int16);
        REQUIRE(n_int16 >= n_int32);

        // All values should be reasonable (2-16)
        REQUIRE(n_int8 >= 2);
        REQUIRE(n_int8 <= 16);
        REQUIRE(n_int64 >= 2);
        REQUIRE(n_int64 <= 16);

        INFO("Sum optimal_N: int8=" << n_int8 << " int16=" << n_int16
             << " int32=" << n_int32 << " int64=" << n_int64);
    }

    SECTION("DotProduct loop type") {
        constexpr auto n_float = optimal_N<LoopType::DotProduct, 4>;
        constexpr auto n_double = optimal_N<LoopType::DotProduct, 8>;

        // Both should be reasonable for hiding FMA latency
        REQUIRE(n_float >= 4);
        REQUIRE(n_double >= 4);

        INFO("DotProduct optimal_N: float=" << n_float << " double=" << n_double);
    }

    SECTION("Search loop type") {
        constexpr auto n_int8 = optimal_N<LoopType::Search, 1>;
        constexpr auto n_int32 = optimal_N<LoopType::Search, 4>;

        // Search should have moderate unrolling (early exit)
        REQUIRE(n_int8 <= 8);
        REQUIRE(n_int32 <= 8);

        INFO("Search optimal_N: int8=" << n_int8 << " int32=" << n_int32);
    }

    SECTION("Copy loop type") {
        constexpr auto n_int8 = optimal_N<LoopType::Copy, 1>;
        constexpr auto n_int32 = optimal_N<LoopType::Copy, 4>;

        REQUIRE(n_int8 >= n_int32);  // Smaller types benefit more

        INFO("Copy optimal_N: int8=" << n_int8 << " int32=" << n_int32);
    }

    SECTION("Transform loop type") {
        constexpr auto n_int8 = optimal_N<LoopType::Transform, 1>;
        constexpr auto n_int32 = optimal_N<LoopType::Transform, 4>;

        REQUIRE(n_int8 >= 2);
        REQUIRE(n_int32 >= 2);

        INFO("Transform optimal_N: int8=" << n_int8 << " int32=" << n_int32);
    }

    SECTION("Default template fallback") {
        // Non-specialized should use default of 4
        constexpr auto n_default = optimal_N<LoopType::Sum, 3>;  // 3-byte not specialized
        REQUIRE(n_default == 4);
    }
}

TEST_CASE("optimal_N with actual types", "[cpu]") {
    // Test with sizeof actual types
    constexpr auto n_char = optimal_N<LoopType::Sum, sizeof(char)>;
    constexpr auto n_short = optimal_N<LoopType::Sum, sizeof(short)>;
    constexpr auto n_int = optimal_N<LoopType::Sum, sizeof(int)>;
    constexpr auto n_long = optimal_N<LoopType::Sum, sizeof(long long)>;
    constexpr auto n_float = optimal_N<LoopType::Sum, sizeof(float)>;
    constexpr auto n_double = optimal_N<LoopType::Sum, sizeof(double)>;

    INFO("Sum with real types: char=" << n_char << " short=" << n_short
         << " int=" << n_int << " long=" << n_long
         << " float=" << n_float << " double=" << n_double);

    REQUIRE(n_char >= 8);   // 1 byte
    REQUIRE(n_short >= 4);  // 2 bytes
    REQUIRE(n_int >= 2);    // 4 bytes
    REQUIRE(n_float >= 2);  // 4 bytes
    REQUIRE(n_double >= 2); // 8 bytes
}

TEST_CASE("Auto-selecting loop functions", "[cpu][auto]") {
    SECTION("for_loop_sum with int") {
        std::vector<int> data(100);
        std::iota(data.begin(), data.end(), 1);

        int sum = 0;
        for_loop_sum(0, 100, [&](int i) {
            sum += data[i];
        });

        REQUIRE(sum == 5050);
    }

    SECTION("for_loop_sum with short") {
        std::vector<short> data(100);
        for (int i = 0; i < 100; ++i) data[i] = static_cast<short>(i + 1);

        short sum = 0;
        for_loop_sum(short(0), short(100), [&](short i) {
            sum += data[i];
        });

        REQUIRE(sum == 5050);
    }

    SECTION("for_loop_range_sum") {
        std::vector<int> data(100);
        std::iota(data.begin(), data.end(), 1);

        int sum = 0;
        for_loop_range_sum(data, [&](int val) {
            sum += val;
        });

        REQUIRE(sum == 5050);
    }

    SECTION("for_loop_search") {
        std::vector<int> data(100);
        std::iota(data.begin(), data.end(), 0);
        data[42] = 999;

        auto result = for_loop_search<int>(0, 100, [&](int i, auto& ctrl) {
            if (data[i] == 999) {
                ctrl.return_with(i);
            }
        });

        REQUIRE(result.has_value());
        REQUIRE(result.value() == 42);
    }

    SECTION("for_loop_range_search") {
        std::vector<int> data = {1, 2, 3, 42, 5, 6};

        auto result = for_loop_range_search<int>(data, [&](int val, auto& ctrl) {
            if (val == 42) {
                ctrl.return_with(val);
            }
        });

        REQUIRE(result.has_value());
        REQUIRE(result.value() == 42);
    }

    SECTION("for_loop_sum not found") {
        std::vector<int> data(100);
        std::iota(data.begin(), data.end(), 0);

        auto result = for_loop_search<int>(0, 100, [&](int i, auto& ctrl) {
            if (data[i] == 999) {  // Not in data
                ctrl.return_with(i);
            }
        });

        REQUIRE_FALSE(result.has_value());
    }
}
