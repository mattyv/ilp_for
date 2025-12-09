#include "catch.hpp"
#include "ilp_for.hpp"
#include <cstdint>
#include <numeric>
#include <vector>

#if !defined(ILP_MODE_SIMPLE)

using namespace ilp;

TEST_CASE("CPU profile optimal_N values", "[cpu]") {
    SECTION("Sum loop type - integers") {
        // Verify we can access optimal_N at compile time with actual types
        constexpr auto n_int8 = optimal_N<LoopType::Sum, int8_t>;
        constexpr auto n_int16 = optimal_N<LoopType::Sum, int16_t>;
        constexpr auto n_int32 = optimal_N<LoopType::Sum, int32_t>;
        constexpr auto n_int64 = optimal_N<LoopType::Sum, int64_t>;

        // All values should be reasonable (2-16)
        REQUIRE(n_int8 >= 2);
        REQUIRE(n_int8 <= 16);
        REQUIRE(n_int64 >= 2);
        REQUIRE(n_int64 <= 16);

        INFO("Sum optimal_N (int): int8=" << n_int8 << " int16=" << n_int16 << " int32=" << n_int32
                                          << " int64=" << n_int64);
    }

    SECTION("Sum loop type - float vs int distinction") {
        // Verify TMP distinguishes float from int for 4-byte and 8-byte types
        constexpr auto n_float = optimal_N<LoopType::Sum, float>;
        constexpr auto n_int32 = optimal_N<LoopType::Sum, int32_t>;
        constexpr auto n_double = optimal_N<LoopType::Sum, double>;
        constexpr auto n_int64 = optimal_N<LoopType::Sum, int64_t>;

        // Float typically has higher latency, so may need more unrolling
        INFO("Sum optimal_N: float=" << n_float << " int32=" << n_int32 << " double=" << n_double
                                     << " int64=" << n_int64);

        REQUIRE(n_float >= 2);
        REQUIRE(n_double >= 2);
    }

    SECTION("DotProduct loop type") {
        constexpr auto n_float = optimal_N<LoopType::DotProduct, float>;
        constexpr auto n_double = optimal_N<LoopType::DotProduct, double>;

        // Both should be reasonable for hiding FMA latency
        REQUIRE(n_float >= 4);
        REQUIRE(n_double >= 4);

        INFO("DotProduct optimal_N: float=" << n_float << " double=" << n_double);
    }

    SECTION("Search loop type") {
        constexpr auto n_int8 = optimal_N<LoopType::Search, int8_t>;
        constexpr auto n_int32 = optimal_N<LoopType::Search, int32_t>;

        // Search should have moderate unrolling (early exit)
        REQUIRE(n_int8 <= 8);
        REQUIRE(n_int32 <= 8);

        INFO("Search optimal_N: int8=" << n_int8 << " int32=" << n_int32);
    }

    SECTION("Copy loop type") {
        constexpr auto n_int8 = optimal_N<LoopType::Copy, int8_t>;
        constexpr auto n_int32 = optimal_N<LoopType::Copy, int32_t>;

        REQUIRE(n_int8 >= n_int32); // Smaller types benefit more

        INFO("Copy optimal_N: int8=" << n_int8 << " int32=" << n_int32);
    }

    SECTION("Transform loop type") {
        constexpr auto n_int8 = optimal_N<LoopType::Transform, int8_t>;
        constexpr auto n_int32 = optimal_N<LoopType::Transform, int32_t>;

        REQUIRE(n_int8 >= 2);
        REQUIRE(n_int32 >= 2);

        INFO("Transform optimal_N: int8=" << n_int8 << " int32=" << n_int32);
    }

    SECTION("Default template fallback") {
        // 3-byte struct not specialized, should use default of 4
        struct ThreeByte {
            char data[3];
        };
        constexpr auto n_default = optimal_N<LoopType::Sum, ThreeByte>;
        REQUIRE(n_default == 4);
    }

    SECTION("Multiply loop type") {
        constexpr auto n_float = optimal_N<LoopType::Multiply, float>;
        constexpr auto n_double = optimal_N<LoopType::Multiply, double>;
        constexpr auto n_int32 = optimal_N<LoopType::Multiply, int32_t>;

        REQUIRE(n_float >= 2);
        REQUIRE(n_double >= 2);
        REQUIRE(n_int32 >= 2);

        INFO("Multiply optimal_N: float=" << n_float << " double=" << n_double << " int32=" << n_int32);
    }

    SECTION("Divide loop type") {
        constexpr auto n_float = optimal_N<LoopType::Divide, float>;
        constexpr auto n_double = optimal_N<LoopType::Divide, double>;

        // Divide is throughput-limited, lower N expected
        REQUIRE(n_float >= 2);
        REQUIRE(n_float <= 8);
        REQUIRE(n_double >= 2);

        INFO("Divide optimal_N: float=" << n_float << " double=" << n_double);
    }

    SECTION("Sqrt loop type") {
        constexpr auto n_float = optimal_N<LoopType::Sqrt, float>;
        constexpr auto n_double = optimal_N<LoopType::Sqrt, double>;

        // Sqrt is throughput-limited like divide
        REQUIRE(n_float >= 2);
        REQUIRE(n_float <= 8);
        REQUIRE(n_double >= 2);

        INFO("Sqrt optimal_N: float=" << n_float << " double=" << n_double);
    }

    SECTION("MinMax loop type") {
        constexpr auto n_float = optimal_N<LoopType::MinMax, float>;
        constexpr auto n_int32 = optimal_N<LoopType::MinMax, int32_t>;
        constexpr auto n_int8 = optimal_N<LoopType::MinMax, int8_t>;

        REQUIRE(n_float >= 2);
        REQUIRE(n_int32 >= 2);
        REQUIRE(n_int8 >= 2);

        INFO("MinMax optimal_N: float=" << n_float << " int32=" << n_int32 << " int8=" << n_int8);
    }

    SECTION("Bitwise loop type") {
        constexpr auto n_int8 = optimal_N<LoopType::Bitwise, int8_t>;
        constexpr auto n_int32 = optimal_N<LoopType::Bitwise, int32_t>;
        constexpr auto n_int64 = optimal_N<LoopType::Bitwise, int64_t>;

        // Bitwise ops are low latency
        REQUIRE(n_int8 >= 2);
        REQUIRE(n_int32 >= 2);
        REQUIRE(n_int64 >= 2);

        INFO("Bitwise optimal_N: int8=" << n_int8 << " int32=" << n_int32 << " int64=" << n_int64);
    }

    SECTION("Shift loop type") {
        constexpr auto n_int8 = optimal_N<LoopType::Shift, int8_t>;
        constexpr auto n_int32 = optimal_N<LoopType::Shift, int32_t>;
        constexpr auto n_int64 = optimal_N<LoopType::Shift, int64_t>;

        REQUIRE(n_int8 >= 2);
        REQUIRE(n_int32 >= 2);
        REQUIRE(n_int64 >= 2);

        INFO("Shift optimal_N: int8=" << n_int8 << " int32=" << n_int32 << " int64=" << n_int64);
    }
}

TEST_CASE("optimal_N with actual types", "[cpu]") {
    // Test with actual C++ types
    constexpr auto n_char = optimal_N<LoopType::Sum, char>;
    constexpr auto n_short = optimal_N<LoopType::Sum, short>;
    constexpr auto n_int = optimal_N<LoopType::Sum, int>;
    constexpr auto n_long = optimal_N<LoopType::Sum, long long>;
    constexpr auto n_float = optimal_N<LoopType::Sum, float>;
    constexpr auto n_double = optimal_N<LoopType::Sum, double>;

    INFO("Sum with real types: char=" << n_char << " short=" << n_short << " int=" << n_int << " long=" << n_long
                                      << " float=" << n_float << " double=" << n_double);

    REQUIRE(n_char >= 2);
    REQUIRE(n_short >= 2);
    REQUIRE(n_int >= 2);
    REQUIRE(n_float >= 2);
    REQUIRE(n_double >= 2);
}

TEST_CASE("Reduce functions with optimal unrolling", "[cpu][auto]") {
    SECTION("reduce with int") {
        std::vector<int> data(100);
        std::iota(data.begin(), data.end(), 1);

        auto sum =
            ilp::reduce<4>(0, 100, int64_t{}, std::plus<>{}, [&](int i) { return static_cast<int64_t>(data[i]); });

        REQUIRE(sum == 5050);
    }

    SECTION("reduce with short") {
        std::vector<short> data(100);
        for (int i = 0; i < 100; ++i)
            data[i] = static_cast<short>(i + 1);

        auto sum = ilp::reduce<4>(short(0), short(100), int64_t{}, std::plus<>{},
                                  [&](short i) { return static_cast<int64_t>(data[i]); });

        REQUIRE(sum == 5050);
    }

    SECTION("reduce_range") {
        std::vector<int> data(100);
        std::iota(data.begin(), data.end(), 1);

        auto sum =
            ilp::reduce_range<4>(data, int64_t{}, std::plus<>{}, [&](int val) { return static_cast<int64_t>(val); });

        REQUIRE(sum == 5050);
    }

#if !defined(ILP_MODE_SIMPLE)
    SECTION("for_loop with return type search") {
        std::vector<int> data(100);
        std::iota(data.begin(), data.end(), 0);
        data[42] = 999;

        auto result = for_loop<4>(0, 100, [&](int i, ForCtrl& ctrl) {
            if (data[i] == 999) {
                ctrl.storage.set(i);
                ctrl.return_set = true;
                ctrl.ok = false;
            }
        });

        REQUIRE(result.has_return);
        int value = *std::move(result);
        REQUIRE(value == 42);
    }

    SECTION("for_loop_range with return type search") {
        std::vector<int> data = {1, 2, 3, 42, 5, 6};

        auto result = for_loop_range<4>(data, [&](int val, ForCtrl& ctrl) {
            if (val == 42) {
                ctrl.storage.set(val);
                ctrl.return_set = true;
                ctrl.ok = false;
            }
        });

        REQUIRE(result.has_return);
        int value = *std::move(result);
        REQUIRE(value == 42);
    }

    SECTION("search not found") {
        std::vector<int> data(100);
        std::iota(data.begin(), data.end(), 0);

        auto result = for_loop<4>(0, 100, [&](int i, ForCtrl& ctrl) {
            if (data[i] == 999) { // Not in data
                ctrl.storage.set(i);
                ctrl.return_set = true;
                ctrl.ok = false;
            }
        });

        REQUIRE_FALSE(result.has_return);
    }
#endif
}

#endif // !ILP_MODE_SIMPLE
