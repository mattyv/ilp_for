#include "../../ilp_for/detail/iota.hpp"
#include "catch.hpp"
#include <vector>

TEST_CASE("iota creates correct range") {
    auto view = ilp::iota(0, 5);
    std::vector<int> values;
    for (auto it = view.begin(); it != view.end(); ++it) {
        values.push_back(*it);
    }
    CHECK(values == std::vector<int>{0, 1, 2, 3, 4});
}

TEST_CASE("iota works with different integral types") {
    SECTION("size_t") {
        auto view = ilp::iota(size_t{0}, size_t{3});
        size_t sum = 0;
        for (auto it = view.begin(); it != view.end(); ++it) {
            sum += *it;
        }
        CHECK(sum == 3); // 0 + 1 + 2
    }
    SECTION("negative range") {
        auto view = ilp::iota(-2, 2);
        std::vector<int> values;
        for (auto it = view.begin(); it != view.end(); ++it) {
            values.push_back(*it);
        }
        CHECK(values == std::vector<int>{-2, -1, 0, 1});
    }
}

TEST_CASE("iota empty range") {
    auto view = ilp::iota(5, 5);
    CHECK_FALSE(view.begin() != view.end());
}
