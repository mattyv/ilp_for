#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <numeric>
#include <cstdint>

#if !defined(ILP_MODE_SIMPLE)

TEST_CASE("Reduce with Break", "[reduce][control]") {
    SECTION("Range-based reduce with break stops correctly") {
        std::vector<int> data = {1, 1, 1, 1,   // Block 1 (sum = 4)
                                 1, 1, 9, 1,   // Block 2 (break at 9)
                                 1, 1, 1, 1};  // Should not be processed

        auto result = ilp::reduce_range<4>(data, 0, std::plus<int>(),
            [](auto&& val) -> std::optional<int> {
                if (val > 5) return std::nullopt;
                return val;
            });

        REQUIRE(result == 6);
    }

    SECTION("Index-based reduce with break stops correctly") {
        auto result = ilp::reduce<4>(0, 100, 0, std::plus<int>(),
            [](auto i) -> std::optional<int> {
                if (i >= 10) return std::nullopt;
                return static_cast<int>(i);
            });

        int expected_sum = 0;
        for(int i = 0; i < 10; ++i) {
            expected_sum += i;
        }

        REQUIRE(result == expected_sum);
    }
}

TEST_CASE("Simple Reduce", "[reduce]") {
    SECTION("Sum of 0 to 99") {
        auto result = ilp::reduce<4>(0, 100, int64_t{0}, std::plus<>{}, [](auto i) {
            return static_cast<int64_t>(i);
        });

        int64_t expected = 0;
        for(int i = 0; i < 100; ++i) expected += i;

        REQUIRE(result == expected);
    }

    SECTION("Sum of a vector") {
        std::vector<int> data(100);
        std::iota(data.begin(), data.end(), 0);

        auto result = ilp::reduce_range<8>(data, int64_t{0}, std::plus<>{}, [](auto&& val) {
            return static_cast<int64_t>(val);
        });

        int64_t expected = 0;
        for(int val : data) expected += val;

        REQUIRE(result == expected);
    }
}

TEST_CASE("Bitwise Reduce Operations", "[reduce][bitwise]") {
    SECTION("Bitwise AND reduction") {
        std::vector<unsigned> data = {0xFF, 0xF0, 0x3F, 0x0F};

        auto result = ilp::reduce_range<4>(data, 0xFFu, std::bit_and<>(), [](auto&& val) {
            return val;
        });

        unsigned expected = 0xFF;
        for(auto v : data) expected &= v;

        REQUIRE(result == expected);
        REQUIRE(result == 0x00);
    }

    SECTION("Bitwise OR reduction") {
        std::vector<unsigned> data = {0x01, 0x02, 0x04, 0x08};

        auto result = ilp::reduce_range<4>(data, 0u, std::bit_or<>(), [](auto&& val) {
            return val;
        });

        unsigned expected = 0;
        for(auto v : data) expected |= v;

        REQUIRE(result == expected);
        REQUIRE(result == 0x0F);
    }

    SECTION("Bitwise XOR reduction") {
        std::vector<unsigned> data = {0xFF, 0xFF, 0x0F, 0x0F};

        auto result = ilp::reduce_range<4>(data, 0u, std::bit_xor<>(), [](auto&& val) {
            return val;
        });

        unsigned expected = 0;
        for(auto v : data) expected ^= v;

        REQUIRE(result == expected);
        REQUIRE(result == 0x00);
    }

    SECTION("Bitwise AND with index-based loop") {
        auto result = ilp::reduce<4>(0, 10, 0xFFFFFFFFu, std::bit_and<>(), [](auto i) {
            return ~(1u << i);
        });

        unsigned expected = 0xFFFFFFFF;
        for(unsigned i = 0; i < 10; ++i) {
            expected &= ~(1u << i);
        }

        REQUIRE(result == expected);
    }
}

TEST_CASE("Cleanup loops with remainders", "[reduce][cleanup]") {
    SECTION("Range reduce with remainder hits cleanup") {
        std::vector<int> data = {1, 1, 1, 1, 1, 1, 1, 1, 1};

        auto result = ilp::reduce_range<4>(data, 0, std::plus<>{}, [](auto&& val) {
            return val;
        });

        REQUIRE(result == 9);
    }

    SECTION("Range reduce SIMPLE with std::plus<>") {
        std::vector<int> data = {1, 2, 3, 4, 5};

        auto result = ilp::reduce_range<4>(data, 0, std::plus<>(), [](auto&& val) {
            return val;
        });

        REQUIRE(result == 15);
    }

    SECTION("Index reduce SIMPLE with std::plus<>") {
        auto result = ilp::reduce<4>(0, 10, 0, std::plus<>(), [](auto i) {
            return i;
        });

        REQUIRE(result == 45);
    }

    SECTION("Vector<double> reduce SIMPLE with std::plus<>") {
        std::vector<double> data = {1.5, 2.5, 3.5, 4.5, 5.5};

        auto result = ilp::reduce_range<4>(data, 0.0, std::plus<>(), [](auto&& val) {
            return val;
        });

        REQUIRE(result == 17.5);
    }

    SECTION("Array<int, 7> reduce SIMPLE with std::plus<>") {
        std::array<int, 7> data = {1, 2, 3, 4, 5, 6, 7};

        auto result = ilp::reduce_range<4>(data, 0, std::plus<>(), [](auto&& val) {
            return val;
        });

        REQUIRE(result == 28);
    }

    SECTION("Unsigned int reduce SIMPLE with std::plus<> and N=4") {
        auto result = ilp::reduce<4>(0u, 10u, 0u, std::plus<>(), [](auto i) {
            return i;
        });

        REQUIRE(result == 45u);
    }

    SECTION("Int reduce SIMPLE with std::plus<> and N=1") {
        auto result = ilp::reduce<1>(0, 10, 0, std::plus<>(), [](auto i) {
            return i;
        });

        REQUIRE(result == 45);
    }

    SECTION("Index reduce with remainder") {
        auto result = ilp::reduce<4>(0, 11, 0, std::plus<>{}, [](auto) {
            return 1;
        });

        REQUIRE(result == 11);
    }

    SECTION("Reduce with break in cleanup loop") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9};

        auto result = ilp::reduce_range<4>(data, 0, std::plus<>(),
            [](auto&& val) -> std::optional<int> {
                if (val == 9) return std::nullopt;
                return val;
            });

        REQUIRE(result == 36);
    }

    SECTION("FOR_RANGE with return type cleanup loop") {
        auto helper = [](const std::vector<int>& data) -> std::optional<int> {
            ILP_FOR_RANGE(auto&& val, data, 4) {
                if (val == 7) {
                    ILP_RETURN(val * 10);
                }
            } ILP_END_RETURN;
            return std::nullopt;
        };

        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7};
        auto result = helper(data);

        REQUIRE(result.has_value());
        REQUIRE(result.value() == 70);
    }
}

// =============================================================================
// Path detection tests - verify std::optional detection
// =============================================================================

TEST_CASE("std::optional is detected correctly", "[reduce][path]") {
    using namespace ilp::detail;

    // std::optional<T> for early break support
    auto lambda = [](int i) -> std::optional<int> {
        return i;
    };
    using R = std::invoke_result_t<decltype(lambda), int>;
    static_assert(is_optional_v<R>, "Should detect std::optional");

    auto result = ilp::reduce_auto(0, 10, 0, std::plus<>{}, [](auto i) {
        return i;
    });
    CHECK(result == 45);
}

TEST_CASE("std::optional with nullopt stops correctly", "[reduce][path]") {
    auto result = ilp::reduce_auto(0, 100, 0, std::plus<>{},
        [](auto i) -> std::optional<int> {
            if (i >= 10) return std::nullopt;
            return i;
        });
    CHECK(result == 45);  // 0+1+...+9
}

TEST_CASE("Range-based reduce auto", "[reduce][path][range]") {
    std::vector<int> data = {0, 1, 2, 3, 4};

    auto result = ilp::reduce_range_auto(data, 0, std::plus<>{}, [](auto val) {
        return val;
    });
    CHECK(result == 10);
}

TEST_CASE("Range-based reduce with nullopt stops correctly", "[reduce][path][range]") {
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    auto result = ilp::reduce_range_auto(data, 0, std::plus<>{},
        [](auto val) -> std::optional<int> {
            if (val >= 5) return std::nullopt;
            return val;
        });
    CHECK(result == 10);  // 0+1+2+3+4
}

#endif
