#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <vector>

// Coverage for the ILP_END / ILP_END_RETURN compile-time enforcement mechanism
// (see docs/END_ENFORCEMENT_PLAN.md). The negative cases - ILP_RETURN closed with
// plain ILP_END, and ilp::for_each's return_with() - live in
// tests/compile_fail/, since they must fail to compile. This file covers the
// positive cases: mixed nesting of ILP_END and ILP_END_RETURN works correctly,
// since the opening/closing macros dispatch on a tag appended by the *closing*
// macro, independent of any enclosing loop's flavor. Nested ILP_RETURN
// propagation (an inner loop's ILP_RETURN escaping through an outer
// ILP_END_RETURN-closed loop) has its own, more thorough coverage in
// tests/correctness/test_nested_return.cpp; the negative case for that
// mechanism - an inner ILP_RETURN whose value has nowhere to go because an
// enclosing loop is ILP_END-closed - lives in
// tests/compile_fail/nested_return_in_end_loop.cpp.

TEST_CASE("ILP_END nested inside ILP_END_RETURN", "[end_enforcement][nesting]") {
    auto find_pair_sum = [](const std::vector<int>& data, int target) -> int {
        ILP_FOR(auto i, std::size_t{0}, data.size(), 4) {
            int local_sum = 0;
            // Inner loop is break-only: closed with plain ILP_END.
            ILP_FOR(auto j, std::size_t{0}, data.size(), 2) {
                if (i == j)
                    ILP_CONTINUE;
                local_sum += data[j];
            }
            ILP_END;
            if (local_sum == target)
                ILP_RETURN(static_cast<int>(i));
        }
        ILP_END_RETURN;
        return -1;
    };

    std::vector<int> data = {1, 2, 3, 4};
    // sum(all) - data[i] == target  =>  10 - data[i] == target
    REQUIRE(find_pair_sum(data, 10 - 3) == 2); // i where data[i] == 3
    REQUIRE(find_pair_sum(data, 999) == -1);
}

TEST_CASE("ILP_END_RETURN nested inside ILP_END_RETURN", "[end_enforcement][nesting]") {
    // An inner loop's ILP_RETURN escapes through an outer ILP_END_RETURN-closed
    // loop's body and returns from the true enclosing function. See
    // test_nested_return.cpp for the fuller battery (3-level nesting, typed
    // combinations, etc.) - this is a minimal smoke test tying the mechanism
    // back to the END-enforcement tag dispatch covered by this file.
    auto find_first_match = [](const std::vector<std::vector<int>>& rows, int target) -> int {
        ILP_FOR(auto r, std::size_t{0}, rows.size(), 2) {
            const auto& row = rows[r];
            ILP_FOR(auto c, std::size_t{0}, row.size(), 4) {
                if (row[c] == target)
                    ILP_RETURN(static_cast<int>(r * 100 + c));
            }
            ILP_END_RETURN;
        }
        ILP_END_RETURN;
        return -1;
    };

    std::vector<std::vector<int>> rows = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    REQUIRE(find_first_match(rows, 6) == 102); // row 1, col 2
    REQUIRE(find_first_match(rows, 42) == -1);
}

TEST_CASE("ilp::for_each nested inside ILP_FOR/ILP_END_RETURN", "[end_enforcement][nesting]") {
    auto find_row_with_negative = [](const std::vector<std::vector<int>>& rows) -> int {
        ILP_FOR(auto r, std::size_t{0}, rows.size(), 2) {
            bool has_negative = false;
            ilp::for_each<4>(std::size_t{0}, rows[r].size(), [&](std::size_t c, auto& ctrl) {
                if (rows[r][c] < 0) {
                    has_negative = true;
                    ctrl.break_loop();
                }
            });
            if (has_negative)
                ILP_RETURN(static_cast<int>(r));
        }
        ILP_END_RETURN;
        return -1;
    };

    std::vector<std::vector<int>> rows = {{1, 2, 3}, {4, -5, 6}, {7, 8, 9}};
    REQUIRE(find_row_with_negative(rows) == 1);

    std::vector<std::vector<int>> all_positive = {{1, 2}, {3, 4}};
    REQUIRE(find_row_with_negative(all_positive) == -1);
}

TEST_CASE("ILP_FOR_T with ILP_END (no ILP_RETURN) still compiles and runs",
          "[end_enforcement][typed]") {
    // A typed loop closed with plain ILP_END and no ILP_RETURN in the body is
    // legal (if pointless) - the end_tag_t overload delegates to the untyped
    // for_each path regardless of the declared return type.
    struct Result {
        int index;
    };

    int sum = 0;
    ILP_FOR_T(Result, auto i, 0, 10, 4) {
        if (i >= 5)
            ILP_BREAK;
        sum += i;
    }
    ILP_END;
    REQUIRE(sum == 10); // 0+1+2+3+4
}
