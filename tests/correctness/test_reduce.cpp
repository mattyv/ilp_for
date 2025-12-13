#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <cstdint>
#include <numeric>
#include <vector>

#if !defined(ILP_MODE_SIMPLE)

// ============================================================================
// Direct reduce tests (new API - no transform)
// ============================================================================

TEST_CASE("Direct reduce (no transform)", "[reduce][direct]") {
    SECTION("Sum of a vector") {
        std::vector<int64_t> data(100);
        std::iota(data.begin(), data.end(), 0);

        auto result = ilp::reduce<4>(data, int64_t{0}, std::plus<>{});

        int64_t expected = 0;
        for (auto val : data)
            expected += val;

        REQUIRE(result == expected);
    }

    SECTION("Sum with different N values") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

        auto result2 = ilp::reduce<2>(data, 0, std::plus<>{});
        auto result4 = ilp::reduce<4>(data, 0, std::plus<>{});
        auto result8 = ilp::reduce<8>(data, 0, std::plus<>{});

        REQUIRE(result2 == 55);
        REQUIRE(result4 == 55);
        REQUIRE(result8 == 55);
    }

    SECTION("Product of small vector") {
        std::vector<int> data = {1, 2, 3, 4, 5};

        auto result = ilp::reduce<4>(data, 1, std::multiplies<>{});

        REQUIRE(result == 120); // 5!
    }
}

TEST_CASE("Direct reduce with auto N selection", "[reduce][direct][auto]") {
    SECTION("reduce_auto sum") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

        auto result = ilp::reduce_auto<ilp::LoopType::Sum>(data, 0, std::plus<>{});

        REQUIRE(result == 55);
    }
}

// ============================================================================
// transform_reduce tests (transform then reduce)
// ============================================================================

TEST_CASE("transform_reduce with transformation", "[transform_reduce]") {
    SECTION("Sum of squares") {
        std::vector<int> data = {1, 2, 3, 4, 5};

        auto result = ilp::transform_reduce<4>(data, 0, std::plus<>{}, [](auto&& val) { return val * val; });

        REQUIRE(result == 55); // 1+4+9+16+25
    }

    SECTION("Sum with type conversion") {
        std::vector<int> data(100);
        std::iota(data.begin(), data.end(), 0);

        auto result = ilp::transform_reduce<8>(data, int64_t{0}, std::plus<>{},
                                               [](auto&& val) { return static_cast<int64_t>(val); });

        int64_t expected = 0;
        for (int val : data)
            expected += val;

        REQUIRE(result == expected);
    }
}

TEST_CASE("transform_reduce with Break (early exit)", "[transform_reduce][control]") {
    SECTION("Range-based transform_reduce with break stops correctly") {
        std::vector<int> data = {1, 1, 1, 1,  // Block 1 (sum = 4)
                                 1, 1, 9, 1,  // Block 2 (break at 9)
                                 1, 1, 1, 1}; // Should not be processed

        auto result = ilp::transform_reduce<4>(data, 0, std::plus<int>(), [](auto&& val) -> std::optional<int> {
            if (val > 5)
                return std::nullopt;
            return val;
        });

        REQUIRE(result == 6);
    }
}

TEST_CASE("Bitwise Reduce Operations", "[reduce][bitwise]") {
    SECTION("Bitwise AND reduction") {
        std::vector<unsigned> data = {0xFF, 0xF0, 0x3F, 0x0F};

        auto result = ilp::reduce<4>(data, 0xFFu, std::bit_and<>());

        unsigned expected = 0xFF;
        for (auto v : data)
            expected &= v;

        REQUIRE(result == expected);
        REQUIRE(result == 0x00);
    }

    SECTION("Bitwise OR reduction") {
        std::vector<unsigned> data = {0x01, 0x02, 0x04, 0x08};

        auto result = ilp::reduce<4>(data, 0u, std::bit_or<>());

        unsigned expected = 0;
        for (auto v : data)
            expected |= v;

        REQUIRE(result == expected);
        REQUIRE(result == 0x0F);
    }

    SECTION("Bitwise XOR reduction") {
        std::vector<unsigned> data = {0xFF, 0xFF, 0x0F, 0x0F};

        auto result = ilp::reduce<4>(data, 0u, std::bit_xor<>());

        unsigned expected = 0;
        for (auto v : data)
            expected ^= v;

        REQUIRE(result == expected);
        REQUIRE(result == 0x00);
    }
}

TEST_CASE("Cleanup loops with remainders", "[reduce][cleanup]") {
    SECTION("Reduce with remainder hits cleanup") {
        std::vector<int> data = {1, 1, 1, 1, 1, 1, 1, 1, 1};

        auto result = ilp::reduce<4>(data, 0, std::plus<>{});

        REQUIRE(result == 9);
    }

    SECTION("Reduce SIMPLE with std::plus<>") {
        std::vector<int> data = {1, 2, 3, 4, 5};

        auto result = ilp::reduce<4>(data, 0, std::plus<>());

        REQUIRE(result == 15);
    }

    SECTION("Vector<double> reduce SIMPLE with std::plus<>") {
        std::vector<double> data = {1.5, 2.5, 3.5, 4.5, 5.5};

        auto result = ilp::reduce<4>(data, 0.0, std::plus<>());

        REQUIRE(result == 17.5);
    }

    SECTION("Array<int, 7> reduce SIMPLE with std::plus<>") {
        std::array<int, 7> data = {1, 2, 3, 4, 5, 6, 7};

        auto result = ilp::reduce<4>(data, 0, std::plus<>());

        REQUIRE(result == 28);
    }

    SECTION("transform_reduce with break in cleanup loop") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9};

        auto result = ilp::transform_reduce<4>(data, 0, std::plus<>(), [](auto&& val) -> std::optional<int> {
            if (val == 9)
                return std::nullopt;
            return val;
        });

        REQUIRE(result == 36);
    }

    SECTION("FOR_RANGE with return type cleanup loop") {
        // Note: Using int return type instead of std::optional<int> because
        // MSVC cannot deduce template parameters for conversion operators in return statements
        auto helper = [](const std::vector<int>& data) -> int {
            ILP_FOR_RANGE(auto&& val, data, 4) {
                if (val == 7) {
                    ILP_RETURN(val * 10);
                }
            }
            ILP_END_RETURN;
            return -1; // sentinel for not found
        };

        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7};
        auto result = helper(data);

        REQUIRE(result != -1);
        REQUIRE(result == 70);
    }
}

// =============================================================================
// Path detection tests - verify std::optional detection
// =============================================================================

TEST_CASE("std::optional is detected correctly", "[transform_reduce][path]") {
    using namespace ilp::detail;

    // std::optional<T> for early break support
    auto lambda = [](int i) -> std::optional<int> { return i; };
    using R = std::invoke_result_t<decltype(lambda), int>;
    static_assert(is_optional_v<R>, "Should detect std::optional");

    std::vector<int> data(10);
    std::iota(data.begin(), data.end(), 0);

    auto result = ilp::reduce<4>(data, 0, std::plus<>{});
    CHECK(result == 45);
}

TEST_CASE("transform_reduce with nullopt stops correctly", "[transform_reduce][path]") {
    std::vector<int> data(100);
    std::iota(data.begin(), data.end(), 0);

    auto result = ilp::transform_reduce_auto<ilp::LoopType::Sum>(data, 0, std::plus<>{},
                                                                   [](auto&& val) -> std::optional<int> {
                                                                       if (val >= 10)
                                                                           return std::nullopt;
                                                                       return val;
                                                                   });
    CHECK(result == 45); // 0+1+...+9
}

TEST_CASE("transform_reduce_auto", "[transform_reduce][path][range]") {
    std::vector<int> data = {0, 1, 2, 3, 4};

    auto result = ilp::transform_reduce_auto<ilp::LoopType::Sum>(data, 0, std::plus<>{}, [](auto val) { return val; });
    CHECK(result == 10);
}

TEST_CASE("transform_reduce with nullopt stops correctly (range)", "[transform_reduce][path][range]") {
    std::vector<int> data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    auto result = ilp::transform_reduce_auto<ilp::LoopType::Sum>(data, 0, std::plus<>{},
                                                                   [](auto val) -> std::optional<int> {
                                                                       if (val >= 5)
                                                                           return std::nullopt;
                                                                       return val;
                                                                   });
    CHECK(result == 10); // 0+1+2+3+4
}

#endif
