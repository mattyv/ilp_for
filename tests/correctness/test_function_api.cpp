#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <limits>
#include <optional>
#include <vector>

// Coverage for the macro-free function API (ilp::for_loop and friends), mirroring
// the break/continue/return coverage in test_for_loops.cpp but exercised via
// ForCtrl::break_loop()/return_with() instead of ILP_BREAK/ILP_RETURN, and via
// explicit ilp::Mode template arguments instead of the ILP_MODE_SIMPLE define.
//
// for_loop/for_loop_range return a [[nodiscard]] ForResult even when the body
// never calls return_with(); calls that don't need the result still capture it
// (as [[maybe_unused]]) to avoid a discarded-nodiscard warning. ilp::for_each /
// for_each_range (below) are the break/continue-only alternative: they return
// void, so no such capture is needed. Attempting ctrl.return_with() in a
// for_each body is a compile error (see tests/compile_fail/for_each_return_with.cpp).

TEST_CASE("for_loop basic accumulation", "[function_api][basic]") {
    SECTION("simple sum") {
        int sum = 0;
        [[maybe_unused]] auto r = ilp::for_loop<4>(0, 10, [&](int i, auto& /*ctrl*/) { sum += i; });
        REQUIRE(sum == 45); // 0+1+2+...+9
    }

    SECTION("empty range") {
        int count = 0;
        [[maybe_unused]] auto r = ilp::for_loop<4>(0, 0, [&](int /*i*/, auto& /*ctrl*/) { count++; });
        REQUIRE(count == 0);
    }

    SECTION("N greater than range length") {
        int sum = 0;
        [[maybe_unused]] auto r = ilp::for_loop<8>(0, 3, [&](int i, auto& /*ctrl*/) { sum += i; });
        REQUIRE(sum == 3); // 0+1+2, all handled by the remainder loop
    }
}

TEST_CASE("for_loop with ctrl.break_loop()", "[function_api][break]") {
    SECTION("break exits loop") {
        int sum = 0;
        [[maybe_unused]] auto r = ilp::for_loop<4>(0, 100, [&](int i, auto& ctrl) {
            if (i >= 10)
                return ctrl.break_loop();
            sum += i;
        });
        REQUIRE(sum == 45); // 0+1+2+...+9
    }

    SECTION("break on first iteration") {
        int count = 0;
        [[maybe_unused]] auto r = ilp::for_loop<4>(0, 100, [&](int /*i*/, auto& ctrl) {
            ctrl.break_loop();
        });
        REQUIRE(count == 0);
    }

    SECTION("break inside the remainder loop (N does not divide range)") {
        // Range is 10 with N=4: two unrolled blocks of 4 (0-3, 4-7), then a
        // remainder of {8, 9}. Breaking at i==9 exercises the remainder path.
        int sum = 0;
        [[maybe_unused]] auto r = ilp::for_loop<4>(0, 10, [&](int i, auto& ctrl) {
            if (i == 9)
                return ctrl.break_loop();
            sum += i;
        });
        REQUIRE(sum == 36); // 0+1+...+8
    }
}

TEST_CASE("for_loop with bare return (continue semantics)", "[function_api][continue]") {
    SECTION("skip even numbers") {
        int sum = 0;
        [[maybe_unused]] auto r = ilp::for_loop<4>(0, 10, [&](int i, auto& /*ctrl*/) {
            if (i % 2 == 0)
                return;
            sum += i;
        });
        REQUIRE(sum == 25); // 1+3+5+7+9
    }

    SECTION("skip all") {
        int sum = 0;
        [[maybe_unused]] auto r = ilp::for_loop<4>(0, 10, [&](int /*i*/, auto& /*ctrl*/) { return; });
        REQUIRE(sum == 0);
    }
}

TEST_CASE("for_loop with ctrl.return_with()", "[function_api][return]") {
    SECTION("return value from loop exits function") {
        auto find_and_double = []() -> int {
            auto r = ilp::for_loop<4>(0, 100, [&](int i, auto& ctrl) {
                if (i == 42)
                    return ctrl.return_with(i * 2);
            });
            if (r)
                return *std::move(r);
            return -1;
        };
        REQUIRE(find_and_double() == 84);
    }

    SECTION("no match - fallthrough to default return") {
        auto find_large = []() -> int {
            auto r = ilp::for_loop<4>(0, 10, [&](int i, auto& ctrl) {
                if (i > 100)
                    return ctrl.return_with(i);
            });
            if (r)
                return *std::move(r);
            return -1;
        };
        REQUIRE(find_large() == -1);
    }
}

TEST_CASE("for_loop_typed with ctrl.return_with()", "[function_api][typed]") {
    struct Result {
        int index;
        double value;
    };

    SECTION("typed return exceeding SBO size") {
        auto find_result = [](const std::vector<int>& data, int target) -> Result {
            auto r = ilp::for_loop_typed<Result, 4>(0, static_cast<int>(data.size()), [&](int i, auto& ctrl) {
                if (data[i] == target)
                    return ctrl.return_with(Result{i, data[i] * 1.5});
            });
            if (r)
                return *std::move(r);
            return Result{-1, 0.0};
        };
        std::vector<int> data = {1, 2, 3, 42, 5};
        Result found = find_result(data, 42);
        REQUIRE(found.index == 3);
        REQUIRE(found.value == 63.0);

        Result missing = find_result(data, 99);
        REQUIRE(missing.index == -1);
    }
}

TEST_CASE("for_loop_range with ctrl methods", "[function_api][range]") {
    SECTION("sum via range loop") {
        std::vector<int> data = {1, 2, 3, 4, 5};
        int sum = 0;
        [[maybe_unused]] auto r = ilp::for_loop_range<4>(data, [&](int val, auto& /*ctrl*/) { sum += val; });
        REQUIRE(sum == 15);
    }

    SECTION("break inside range loop") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7};
        int sum = 0;
        [[maybe_unused]] auto r = ilp::for_loop_range<4>(data, [&](int val, auto& ctrl) {
            if (val > 4)
                return ctrl.break_loop();
            sum += val;
        });
        REQUIRE(sum == 10); // 1+2+3+4
    }

    SECTION("empty range") {
        std::vector<int> data;
        int count = 0;
        [[maybe_unused]] auto r = ilp::for_loop_range<4>(data, [&](int /*val*/, auto& /*ctrl*/) { count++; });
        REQUIRE(count == 0);
    }

    SECTION("typed return from range loop") {
        auto find_double = [](const std::vector<int>& data, int target) -> int {
            auto r = ilp::for_loop_range_typed<int, 4>(data, [&](int val, auto& ctrl) {
                if (val == target)
                    return ctrl.return_with(val * 2);
            });
            if (r)
                return *std::move(r);
            return -1;
        };
        std::vector<int> data = {1, 2, 3, 42, 5};
        REQUIRE(find_double(data, 42) == 84);
        REQUIRE(find_double(data, 99) == -1);
    }
}

// Mode tags used by both the for_each and for_loop Mode-equivalence
// TEMPLATE_TEST_CASEs below (Catch2's TEMPLATE_TEST_CASE list can't contain a
// bare comma-bearing type like std::integral_constant<ilp::Mode, ilp::Mode::X>
// directly - the preprocessor would split it into two macro arguments).
using ModeUnrolledTag = std::integral_constant<ilp::Mode, ilp::Mode::Unrolled>;
using ModeSimpleTag = std::integral_constant<ilp::Mode, ilp::Mode::Simple>;

TEST_CASE("for_each basic accumulation", "[function_api][for_each][basic]") {
    SECTION("simple sum") {
        int sum = 0;
        ilp::for_each<4>(0, 10, [&](int i, auto& /*ctrl*/) { sum += i; });
        REQUIRE(sum == 45); // 0+1+2+...+9
    }

    SECTION("empty range") {
        int count = 0;
        ilp::for_each<4>(0, 0, [&](int /*i*/, auto& /*ctrl*/) { count++; });
        REQUIRE(count == 0);
    }

    SECTION("N greater than range length") {
        int sum = 0;
        ilp::for_each<8>(0, 3, [&](int i, auto& /*ctrl*/) { sum += i; });
        REQUIRE(sum == 3); // 0+1+2, all handled by the remainder loop
    }
}

TEST_CASE("for_each does not overflow the unrolled-block bound near INT_MAX",
          "[function_api][for_each][boundary]") {
    // The unrolled block loop's bound used to be computed as `i + N <= end`, which
    // is signed-integer-overflow UB (reproduced under -fsanitize=signed-integer-
    // overflow) once `end` is within N of the type's maximum. It is now precomputed
    // in unsigned arithmetic (see detail::unrolled_block_end in loops_ilp.hpp), so
    // this must run clean under UBSan and still visit every index exactly once.
    SECTION("range ends exactly at INT_MAX") {
        long count = 0;
        int last = 0;
        ilp::for_each<4>(std::numeric_limits<int>::max() - 10, std::numeric_limits<int>::max(),
                          [&](int i, auto& /*ctrl*/) {
                              ++count;
                              last = i;
                          });
        REQUIRE(count == 10);
        REQUIRE(last == std::numeric_limits<int>::max() - 1);
    }

    SECTION("range of exactly N ending at INT_MAX") {
        long count = 0;
        ilp::for_each<4>(std::numeric_limits<int>::max() - 4, std::numeric_limits<int>::max(),
                          [&](int /*i*/, auto& /*ctrl*/) { ++count; });
        REQUIRE(count == 4);
    }
}

TEST_CASE("for_each with ctrl.break_loop()", "[function_api][for_each][break]") {
    SECTION("break exits loop") {
        int sum = 0;
        ilp::for_each<4>(0, 100, [&](int i, auto& ctrl) {
            if (i >= 10)
                return ctrl.break_loop();
            sum += i;
        });
        REQUIRE(sum == 45); // 0+1+2+...+9
    }

    SECTION("break inside the remainder loop (N does not divide range)") {
        int sum = 0;
        ilp::for_each<4>(0, 10, [&](int i, auto& ctrl) {
            if (i == 9)
                return ctrl.break_loop();
            sum += i;
        });
        REQUIRE(sum == 36); // 0+1+...+8
    }
}

TEST_CASE("for_each with bare return (continue semantics)", "[function_api][for_each][continue]") {
    SECTION("skip even numbers") {
        int sum = 0;
        ilp::for_each<4>(0, 10, [&](int i, auto& /*ctrl*/) {
            if (i % 2 == 0)
                return;
            sum += i;
        });
        REQUIRE(sum == 25); // 1+3+5+7+9
    }
}

TEST_CASE("for_each_range with ctrl methods", "[function_api][for_each][range]") {
    SECTION("sum via range loop") {
        std::vector<int> data = {1, 2, 3, 4, 5};
        int sum = 0;
        ilp::for_each_range<4>(data, [&](int val, auto& /*ctrl*/) { sum += val; });
        REQUIRE(sum == 15);
    }

    SECTION("break inside range loop") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7};
        int sum = 0;
        ilp::for_each_range<4>(data, [&](int val, auto& ctrl) {
            if (val > 4)
                return ctrl.break_loop();
            sum += val;
        });
        REQUIRE(sum == 10); // 1+2+3+4
    }

    SECTION("empty range") {
        std::vector<int> data;
        int count = 0;
        ilp::for_each_range<4>(data, [&](int /*val*/, auto& /*ctrl*/) { count++; });
        REQUIRE(count == 0);
    }
}

TEMPLATE_TEST_CASE("for_each Mode::Unrolled and Mode::Simple agree", "[function_api][for_each][mode]",
                    ModeUnrolledTag, ModeSimpleTag) {
    constexpr ilp::Mode M = TestType::value;

    SECTION("sum with break in the middle") {
        int sum = 0;
        ilp::for_each<4, M>(0, 20, [&](int i, auto& ctrl) {
            if (i == 13)
                return ctrl.break_loop();
            sum += i;
        });
        REQUIRE(sum == 78); // 0+1+...+12
    }

    SECTION("range loop break") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7};
        int sum = 0;
        ilp::for_each_range<4, M>(data, [&](int val, auto& ctrl) {
            if (val > 4)
                return ctrl.break_loop();
            sum += val;
        });
        REQUIRE(sum == 10);
    }
}

TEST_CASE("per-loop Mode override on for_each does not require the global define",
          "[function_api][for_each][mode]") {
    int sum = 0;
    ilp::for_each<4, ilp::Mode::Simple>(0, 10, [&](int i, auto& /*ctrl*/) { sum += i; });
    REQUIRE(sum == 45);

    sum = 0;
    ilp::for_each<4, ilp::Mode::Unrolled>(0, 10, [&](int i, auto& /*ctrl*/) { sum += i; });
    REQUIRE(sum == 45);
}

// Mode equivalence: ilp::Mode::Unrolled and ilp::Mode::Simple must be
// observationally identical for every case above, since Simple mode is meant to
// be a debugging aid, not a semantic change.
TEMPLATE_TEST_CASE("for_loop Mode::Unrolled and Mode::Simple agree", "[function_api][mode]", ModeUnrolledTag,
                    ModeSimpleTag) {
    constexpr ilp::Mode M = TestType::value;

    SECTION("sum with break in the middle") {
        int sum = 0;
        [[maybe_unused]] auto r = ilp::for_loop<4, M>(0, 20, [&](int i, auto& ctrl) {
            if (i == 13)
                return ctrl.break_loop();
            sum += i;
        });
        REQUIRE(sum == 78); // 0+1+...+12
    }

    SECTION("continue skips odd numbers") {
        int sum = 0;
        [[maybe_unused]] auto r = ilp::for_loop<4, M>(0, 10, [&](int i, auto& /*ctrl*/) {
            if (i % 2 != 0)
                return;
            sum += i;
        });
        REQUIRE(sum == 20); // 0+2+4+6+8
    }

    SECTION("return_with propagates the stored value") {
        auto r = ilp::for_loop<4, M>(0, 50, [&](int i, auto& ctrl) {
            if (i == 17)
                return ctrl.return_with(i);
        });
        REQUIRE(static_cast<bool>(r));
        REQUIRE(static_cast<int>(*std::move(r)) == 17);
    }

    SECTION("N greater than range length") {
        int sum = 0;
        [[maybe_unused]] auto r = ilp::for_loop<8, M>(0, 3, [&](int i, auto& /*ctrl*/) { sum += i; });
        REQUIRE(sum == 3);
    }

    SECTION("range loop break") {
        std::vector<int> data = {1, 2, 3, 4, 5, 6, 7};
        int sum = 0;
        [[maybe_unused]] auto r = ilp::for_loop_range<4, M>(data, [&](int val, auto& ctrl) {
            if (val > 4)
                return ctrl.break_loop();
            sum += val;
        });
        REQUIRE(sum == 10);
    }
}

TEST_CASE("ilp::default_mode matches ILP_MODE_SIMPLE", "[function_api][mode]") {
#if defined(ILP_MODE_SIMPLE)
    STATIC_REQUIRE(ilp::default_mode == ilp::Mode::Simple);
#else
    STATIC_REQUIRE(ilp::default_mode == ilp::Mode::Unrolled);
#endif
}

TEST_CASE("per-loop Mode override does not require the global define", "[function_api][mode]") {
    // Regardless of how the translation unit was built, an explicit Mode
    // argument always wins over ilp::default_mode.
    int sum = 0;
    [[maybe_unused]] auto r1 =
        ilp::for_loop<4, ilp::Mode::Simple>(0, 10, [&](int i, auto& /*ctrl*/) { sum += i; });
    REQUIRE(sum == 45);

    sum = 0;
    [[maybe_unused]] auto r2 =
        ilp::for_loop<4, ilp::Mode::Unrolled>(0, 10, [&](int i, auto& /*ctrl*/) { sum += i; });
    REQUIRE(sum == 45);
}

TEST_CASE("matched-type return_with/extract round-trip is unaffected by the debug-mode type check",
          "[function_api][typecheck]") {
    // DESIGN_NOTES.md item 3's debug-mode SBO type check (see ctrl.hpp,
    // ILP_TYPECHECK_ENABLED) only aborts on a stored-vs-recovered type
    // mismatch; matched types must keep working exactly as before. The
    // negative cases (mismatched types, which must abort) live in
    // tests/runtime_fail/ since they can't be expressed as a passing
    // Catch2 assertion.
    auto find_and_double = [](const std::vector<int>& v, int target) -> int {
        auto r = ilp::for_loop<4>(0, static_cast<int>(v.size()), [&](int i, auto& ctrl) {
            if (v[i] == target)
                return ctrl.return_with(v[i] * 2);
        });
        if (r)
            return *std::move(r);
        return -1;
    };

    std::vector<int> v = {1, 2, 3, 42, 5};
    REQUIRE(find_and_double(v, 42) == 84);
    REQUIRE(find_and_double(v, 99) == -1);
}
