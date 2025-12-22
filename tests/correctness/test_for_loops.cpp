#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <vector>

// Basic ILP_FOR tests - works in all modes including SUPER_SIMPLE

TEST_CASE("ILP_FOR basic accumulation", "[for][basic]") {
    SECTION("simple sum") {
        int sum = 0;
        ILP_FOR(auto i, 0, 10, 4) {
            sum += i;
        }
        ILP_END;
        REQUIRE(sum == 45); // 0+1+2+...+9
    }

    SECTION("size_t indices") {
        std::size_t sum = 0;
        ILP_FOR(auto i, size_t(0), size_t(100), 8) {
            sum += i;
        }
        ILP_END;
        REQUIRE(sum == 4950);
    }

    SECTION("negative range") {
        int sum = 0;
        ILP_FOR(auto i, -5, 5, 4) {
            sum += i;
        }
        ILP_END;
        REQUIRE(sum == -5); // -5+-4+-3+-2+-1+0+1+2+3+4
    }

    SECTION("empty range") {
        int count = 0;
        ILP_FOR([[maybe_unused]] auto i, 0, 0, 4) {
            count++;
        }
        ILP_END;
        REQUIRE(count == 0);
    }

    SECTION("single element") {
        int sum = 0;
        ILP_FOR(auto i, 5, 6, 4) {
            sum += i;
        }
        ILP_END;
        REQUIRE(sum == 5);
    }
}

TEST_CASE("ILP_FOR with ILP_BREAK", "[for][break]") {
    SECTION("break exits loop") {
        int sum = 0;
        ILP_FOR(auto i, 0, 100, 4) {
            if (i >= 10)
                ILP_BREAK;
            sum += i;
        }
        ILP_END;
        REQUIRE(sum == 45); // 0+1+2+...+9
    }

    SECTION("break on first iteration") {
        int count = 0;
        ILP_FOR([[maybe_unused]] auto i, 0, 100, 4) {
            ILP_BREAK;
            count++;
        }
        ILP_END;
        REQUIRE(count == 0);
    }

    SECTION("break never triggered") {
        int sum = 0;
        ILP_FOR(auto i, 0, 5, 4) {
            if (i > 100)
                ILP_BREAK;
            sum += i;
        }
        ILP_END;
        REQUIRE(sum == 10); // 0+1+2+3+4
    }
}

TEST_CASE("ILP_FOR with ILP_CONTINUE", "[for][continue]") {
    SECTION("skip even numbers") {
        int sum = 0;
        ILP_FOR(auto i, 0, 10, 4) {
            if (i % 2 == 0)
                ILP_CONTINUE;
            sum += i;
        }
        ILP_END;
        REQUIRE(sum == 25); // 1+3+5+7+9
    }

    SECTION("skip all") {
        int sum = 0;
        ILP_FOR(auto i, 0, 10, 4) {
            ILP_CONTINUE;
            sum += i;
        }
        ILP_END;
        REQUIRE(sum == 0);
    }
}

TEST_CASE("ILP_FOR_RANGE basic", "[for_range][basic]") {
    SECTION("vector iteration") {
        std::vector<int> data = {1, 2, 3, 4, 5};
        int sum = 0;
        ILP_FOR_RANGE(auto val, data, 4) {
            sum += val;
        }
        ILP_END;
        REQUIRE(sum == 15);
    }

    SECTION("empty vector") {
        std::vector<int> data;
        int count = 0;
        ILP_FOR_RANGE([[maybe_unused]] auto val, data, 4) {
            count++;
        }
        ILP_END;
        REQUIRE(count == 0);
    }

    SECTION("break in range loop") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        int sum = 0;
        ILP_FOR_RANGE(auto val, data, 4) {
            if (val > 5)
                ILP_BREAK;
            sum += val;
        }
        ILP_END;
        REQUIRE(sum == 15); // 1+2+3+4+5
    }

    SECTION("continue in range loop") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        int sum = 0;
        ILP_FOR_RANGE(auto val, data, 4) {
            if (val % 2 == 0)
                ILP_CONTINUE;
            sum += val;
        }
        ILP_END;
        REQUIRE(sum == 25); // 1+3+5+7+9
    }
}

TEST_CASE("ILP_FOR_AUTO basic", "[for_auto][basic]") {
    SECTION("auto N selection") {
        int sum = 0;
        ILP_FOR_AUTO(auto i, 0, 10, Sum, int) {
            sum += i;
        }
        ILP_END;
        REQUIRE(sum == 45);
    }
}

TEST_CASE("ILP_FOR_RANGE_AUTO basic", "[for_range_auto][basic]") {
    SECTION("auto N with range") {
        std::vector<int> data = {10, 20, 30, 40, 50};
        int sum = 0;
        ILP_FOR_RANGE_AUTO(auto val, data, Sum, int) {
            sum += val;
        }
        ILP_END;
        REQUIRE(sum == 150);
    }
}

// Test ILP_RETURN only in non-SUPER_SIMPLE modes (different semantics)
#if !defined(ILP_MODE_SIMPLE)
TEST_CASE("ILP_FOR with ILP_RETURN", "[for][return]") {
    SECTION("return value from loop exits function") {
        auto find_and_double = []() -> int {
            ILP_FOR(auto i, 0, 100, 4) {
                if (i == 42)
                    ILP_RETURN(i * 2);
            }
            ILP_END_RETURN;
            return -1; // Not found
        };
        REQUIRE(find_and_double() == 84);
    }

    SECTION("no match - fallthrough to default return") {
        auto find_large = []() -> int {
            ILP_FOR(auto i, 0, 10, 4) {
                if (i > 100)
                    ILP_RETURN(i);
            }
            ILP_END_RETURN;
            return -1; // Not found
        };
        REQUIRE(find_large() == -1);
    }

    SECTION("ILP_RETURN with range loop") {
        auto find_value = [](const std::vector<int>& data, int target) -> int {
            ILP_FOR_RANGE(auto val, data, 4) {
                if (val == target)
                    ILP_RETURN(val * 10);
            }
            ILP_END_RETURN;
            return -1;
        };
        std::vector<int> data = {1, 2, 3, 42, 5};
        REQUIRE(find_value(data, 42) == 420);
        REQUIRE(find_value(data, 99) == -1);
    }

    SECTION("ILP_RETURN with AUTO loops") {
        auto find_first_even = []() -> int {
            ILP_FOR_AUTO(auto i, 1, 100, Search, int) {
                if (i % 2 == 0)
                    ILP_RETURN(i);
            }
            ILP_END_RETURN;
            return -1;
        };
        REQUIRE(find_first_even() == 2);
    }
}

TEST_CASE("ILP_FOR_RANGE_T basic", "[for_range_t][basic]") {
    SECTION("typed return from range loop") {
        auto find_double = [](const std::vector<int>& data, int target) -> int {
            ILP_FOR_RANGE_T(int, auto val, data, 4) {
                if (val == target)
                    ILP_RETURN(val * 2);
            }
            ILP_END_RETURN;
            return -1;
        };
        std::vector<int> data = {1, 2, 3, 42, 5};
        REQUIRE(find_double(data, 42) == 84);
        REQUIRE(find_double(data, 99) == -1);
    }
}

TEST_CASE("ILP_FOR_T_AUTO basic", "[for_t_auto][basic]") {
    SECTION("typed return with auto N") {
        auto find_square = []() -> int {
            ILP_FOR_T_AUTO(int, auto i, 1, 20, Search, int) {
                if (i * i == 49)
                    ILP_RETURN(i);
            }
            ILP_END_RETURN;
            return -1;
        };
        REQUIRE(find_square() == 7);
    }
}

TEST_CASE("ILP_FOR_RANGE_T_AUTO basic", "[for_range_t_auto][basic]") {
    SECTION("typed return with auto N and range") {
        auto find_triple = [](const std::vector<int>& data, int target) -> int {
            ILP_FOR_RANGE_T_AUTO(int, auto val, data, Search, int) {
                if (val == target)
                    ILP_RETURN(val * 3);
            }
            ILP_END_RETURN;
            return -1;
        };
        std::vector<int> data = {10, 20, 30, 40, 50};
        REQUIRE(find_triple(data, 30) == 90);
        REQUIRE(find_triple(data, 99) == -1);
    }
}
#endif
