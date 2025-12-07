#include "catch.hpp"
#include "../../ilp_for.hpp"
#include <vector>
#include <limits>
#include <functional>

#if !defined(ILP_MODE_SUPER_SIMPLE)

// =============================================================================
// ACCUMULATOR BUG INVESTIGATION TESTS
// Tests targeting the N×init multiplication bug
// =============================================================================

// -----------------------------------------------------------------------------
// Empty Range Tests - All should return init, not N×init
// -----------------------------------------------------------------------------

TEST_CASE("Empty range - init multiplication bug", "[bug][accumulator]") {
    SECTION("Sum with init 100, empty range") {
        auto result = ilp::reduce<4>(0, 0, 100, std::plus<>(), [&](auto i) {
            return i;
        });
        INFO("Bug: Returns 400 instead of 100 (N=4 × init=100)");
        // EXPECTED: 100 (init unchanged)
        // ACTUAL: 400 (4 × 100)
        REQUIRE(result == 100);  // This will FAIL
    }

    SECTION("Product with init 5, empty range") {
        auto result = ilp::reduce<4>(0, 0, 5, std::multiplies<>(), [&](auto i) {
            return i;
        });
        INFO("Bug: Returns 625 instead of 5 (5^4)");
        // EXPECTED: 5
        // ACTUAL: 5 * 5 * 5 * 5 = 625
        REQUIRE(result == 5);  // This will FAIL
    }

    SECTION("Max with init MIN, empty range") {
        auto result = ilp::reduce<4>(0, 0, std::numeric_limits<int>::min(),
            [](int a, int b) { return std::max(a, b); },
            [&](auto i) {
                return i;
            });
        // max(min, min, min, min) = min - this actually works!
        REQUIRE(result == std::numeric_limits<int>::min());
    }

    SECTION("Min with init MAX, empty range") {
        auto result = ilp::reduce<4>(0, 0, std::numeric_limits<int>::max(),
            [](int a, int b) { return std::min(a, b); },
            [&](auto i) {
                return i;
            });
        // min(max, max, max, max) = max - this actually works!
        REQUIRE(result == std::numeric_limits<int>::max());
    }

    SECTION("XOR with init, empty range") {
        auto result = ilp::reduce<4>(0, 0, 42,
            [](int a, int b) { return a ^ b; },
            [&](auto i) {
                return i;
            });
        INFO("Bug: XOR of 4 copies - 42^42^42^42 = 0");
        // EXPECTED: 42
        // ACTUAL: 0 (xor is self-inverse)
        REQUIRE(result == 42);  // This will FAIL
    }
}

// -----------------------------------------------------------------------------
// Single Element Tests
// -----------------------------------------------------------------------------

TEST_CASE("Single element - init multiplication impact", "[bug][accumulator]") {
    SECTION("Sum with init 100, single element 5") {
        auto result = ilp::reduce<4>(0, 1, 100, std::plus<>(), [&](auto i) {
            return i;  // returns 0
        });
        INFO("Bug: Returns 400 instead of 100 (element 0 is consumed by one accumulator)");
        // EXPECTED: 100 + 0 = 100
        // ACTUAL: (100+0) + 100 + 100 + 100 = 400
        REQUIRE(result == 100);  // This will FAIL
    }

    SECTION("Product with init 2, single element") {
        auto result = ilp::reduce<4>(1, 2, 2, std::multiplies<>(), [&](auto i) {
            return i;
        });
        INFO("Bug: Returns 16 instead of 2 (2*1 * 2 * 2 * 2 = 16)");
        // EXPECTED: 2 * 1 = 2
        // ACTUAL: (2*1) * 2 * 2 * 2 = 16
        REQUIRE(result == 2);  // This will FAIL
    }
}

// -----------------------------------------------------------------------------
// Different N Values
// -----------------------------------------------------------------------------

TEST_CASE("Init multiplication scales with N", "[bug][accumulator]") {
    SECTION("N=1 - should be correct") {
        auto result = ilp::reduce<1>(0, 0, 100, std::plus<>(), [&](auto i) {
            return i;
        });
        REQUIRE(result == 100);  // N=1, so 100*1 = 100
    }

    SECTION("N=2 - init×2") {
        auto result = ilp::reduce<2>(0, 0, 100, std::plus<>(), [&](auto i) {
            return i;
        });
        // Will be 200
        INFO("With N=2, empty range returns 200");
        REQUIRE(result == 100);  // Will FAIL with 200
    }

    SECTION("N=8 - init×8") {
        auto result = ilp::reduce<8>(0, 0, 100, std::plus<>(), [&](auto i) {
            return i;
        });
        // Will be 800
        INFO("With N=8, empty range returns 800");
        REQUIRE(result == 100);  // Will FAIL with 800
    }
}

// -----------------------------------------------------------------------------
// Early Break Tests
// -----------------------------------------------------------------------------

#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA) && !defined(ILP_MODE_SUPER_SIMPLE)

TEST_CASE("Early break - init multiplication", "[bug][accumulator]") {
    SECTION("Break on first iteration") {
        auto result = ilp::reduce<4>(0, 100, 100, std::plus<>(),
            [&](auto) -> std::optional<int> {
                return std::nullopt;  // 0 is identity for plus
            });
        INFO("Bug: Returns 400 instead of 100");
        REQUIRE(result == 100);  // Will FAIL with 400
    }

    SECTION("Break after first element") {
        auto result = ilp::reduce<4>(0, 100, 100, std::plus<>(),
            [&](auto i) -> std::optional<int> {
                if (i == 1) return std::nullopt;
                return i;
            });
        // First iteration processes 0 in one accumulator
        // Then breaks
        INFO("Bug: Returns 400 instead of 100");
        REQUIRE(result == 100);  // Will FAIL
    }
}

#endif

// -----------------------------------------------------------------------------
// Range Reduce Tests
// -----------------------------------------------------------------------------

TEST_CASE("Range reduce - init multiplication", "[bug][accumulator][range]") {
    SECTION("Empty vector") {
        std::vector<int> empty;
        auto result = ilp::reduce_range<4>(empty, 100, std::plus<>(), [&](auto&& val) {
            return val;
        });
        INFO("Bug: Empty vector returns 400");
        REQUIRE(result == 100);  // Will FAIL with 400
    }

    SECTION("Single element vector") {
        std::vector<int> single = {5};
        auto result = ilp::reduce_range<4>(single, 100, std::plus<>(), [&](auto&& val) {
            return val;
        });
        INFO("Bug: Single element returns 405 instead of 105");
        // EXPECTED: 100 + 5 = 105
        // ACTUAL: (100+5) + 100 + 100 + 100 = 405
        REQUIRE(result == 105);  // Will FAIL with 405
    }
}

// -----------------------------------------------------------------------------
// Operations that "work" due to mathematical properties
// -----------------------------------------------------------------------------

TEST_CASE("Operations where bug doesn't manifest", "[accumulator][works]") {
    // These operations have idempotent or self-inverse properties

    SECTION("Max - idempotent") {
        auto result = ilp::reduce<4>(0, 0, -999,
            [](int a, int b) { return std::max(a, b); },
            [&](auto i) {
                return i;
            });
        // max(-999, -999, -999, -999) = -999 - works!
        REQUIRE(result == -999);
    }

    SECTION("Min - idempotent") {
        auto result = ilp::reduce<4>(0, 0, 999,
            [](int a, int b) { return std::min(a, b); },
            [&](auto i) {
                return i;
            });
        // min(999, 999, 999, 999) = 999 - works!
        REQUIRE(result == 999);
    }

    SECTION("AND with all 1s") {
        auto result = ilp::reduce<4>(0, 0, 0xFF,
            [](int a, int b) { return a & b; },
            [&](auto i) {
                return i;
            });
        // 0xFF & 0xFF & 0xFF & 0xFF = 0xFF - works!
        REQUIRE(result == 0xFF);
    }

    SECTION("OR with 0") {
        auto result = ilp::reduce<4>(0, 0, 0,
            [](int a, int b) { return a | b; },
            [&](auto i) {
                return i;
            });
        // 0 | 0 | 0 | 0 = 0 - works!
        REQUIRE(result == 0);
    }
}

// -----------------------------------------------------------------------------
// Correct Behavior with Zero Init
// -----------------------------------------------------------------------------

TEST_CASE("Sum with zero init - correct behavior", "[accumulator][correct]") {
    // Sum with init=0 works correctly because 0+0+0+0 = 0

    SECTION("Empty range") {
        auto result = ilp::reduce<4>(0, 0, 0, std::plus<>{}, [&](auto i) {
            return static_cast<int64_t>(i);
        });
        REQUIRE(result == 0);
    }

    SECTION("Single element") {
        auto result = ilp::reduce<4>(0, 1, 0, std::plus<>{}, [&](auto i) {
            return static_cast<int64_t>(i);
        });
        REQUIRE(result == 0);
    }

    SECTION("Multiple elements") {
        auto result = ilp::reduce<4>(0, 10, 0, std::plus<>{}, [&](auto i) {
            return static_cast<int64_t>(i);
        });
        REQUIRE(result == 45);
    }
}

// -----------------------------------------------------------------------------
// Product with One Init - Correct Behavior
// -----------------------------------------------------------------------------

TEST_CASE("Product with one init - correct behavior", "[accumulator][correct]") {
    // Product with init=1 also works correctly because 1*1*1*1 = 1

    SECTION("Empty range") {
        auto result = ilp::reduce<4>(0, 0, 1, std::multiplies<>(), [&](auto i) {
            return i;
        });
        REQUIRE(result == 1);
    }

    SECTION("Factorial 5") {
        auto result = ilp::reduce<4>(1, 6, 1, std::multiplies<>(), [&](auto i) {
            return i;
        });
        REQUIRE(result == 120);
    }
}

// -----------------------------------------------------------------------------
// Demonstrating the severity of the bug
// -----------------------------------------------------------------------------

TEST_CASE("Bug severity demonstration", "[bug][severity]") {
    SECTION("Off-by-huge-amount for counting") {
        // Trying to count with init offset
        auto result = ilp::reduce<4>(0, 5, 1000, std::plus<>(), [&](auto) {
            return 1;  // Add 1 for each element
        });
        // EXPECTED: 1000 + 5 = 1005
        // ACTUAL: (1000 + 1 + 1 + 1 + 1 + 1) + 1000 + 1000 + 1000 = 4005
        INFO("Counting with offset gives 4005 instead of 1005");
        REQUIRE(result == 1005);  // Will FAIL
    }

    SECTION("Financial calculation error") {
        // Starting balance + sum of transactions
        int starting_balance = 10000;
        std::vector<int> transactions = {};  // No transactions yet

        auto final_balance = ilp::reduce_range<4>(transactions, starting_balance, std::plus<>(), [&](auto&& txn) {
            return txn;
        });

        INFO("Empty transactions should return starting balance 10000, not 40000");
        REQUIRE(final_balance == 10000);  // Will FAIL with 40000
    }
}

// -----------------------------------------------------------------------------
// Workaround Demonstration
// -----------------------------------------------------------------------------

TEST_CASE("Workaround: Handle empty separately", "[workaround]") {
    // Users must check for empty ranges themselves
    std::vector<int> empty;

    int result;
    if (empty.empty()) {
        result = 100;  // Handle empty case manually
    } else {
        result = ilp::reduce_range<4>(empty, 100, std::plus<>(), [&](auto&& val) {
            return val;
        });
    }

    REQUIRE(result == 100);  // Works with workaround
}

// -----------------------------------------------------------------------------
// Testing reduce_sum default init behavior
// -----------------------------------------------------------------------------

TEST_CASE("reduce_sum uses correct zero init", "[accumulator][correct]") {
    // reduce_sum defaults to R{} which is 0 for numeric types

    SECTION("Empty range") {
        auto result = ilp::reduce<4>(0, 0, 0, std::plus<>{}, [&](auto i) {
            return i;
        });
        // 0 + 0 + 0 + 0 = 0 - correct!
        REQUIRE(result == 0);
    }

    SECTION("Range sum") {
        auto result = ilp::reduce<4>(0, 100, 0, std::plus<>{}, [&](auto i) {
            return i;
        });
        REQUIRE(result == 4950);
    }
}

#endif // !ILP_MODE_SUPER_SIMPLE
