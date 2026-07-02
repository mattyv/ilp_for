#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <vector>

// Coverage for the ILP_END / ILP_END_RETURN compile-time enforcement mechanism
// (see docs/END_ENFORCEMENT_PLAN.md). The negative cases - ILP_RETURN closed with
// plain ILP_END, and ilp::for_each's return_with() - live in
// tests/compile_fail/, since they must fail to compile. This file covers the
// positive cases: an ILP_END-closed (break-only) loop nested inside an
// ILP_END_RETURN-closed loop's body works correctly, since the opening/closing
// macros dispatch on a tag appended by the *closing* macro, independent of any
// enclosing loop's flavor. (The reverse nesting - propagating an inner loop's
// ILP_RETURN out through an outer loop's body - has a separate, pre-existing
// limitation unrelated to this feature; see the NOTE further down.)

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

// NOTE: an ILP_FOR/ILP_END_RETURN block nested *inside* another ILP_FOR's body
// does NOT propagate its return value out of the true enclosing C++ function -
// this is a pre-existing architectural limitation of the macro layer, not
// something introduced or fixed by the END enforcement work, and it predates
// this file (reproduces identically against the pre-enforcement library
// snapshot). ILP_END_RETURN's `return *std::move(ilp_detail_ret);` is a bare
// `return` statement textually embedded at whatever scope directly contains
// it; when that scope is itself another ILP_FOR's body lambda (whose return
// type is `auto`-deduced), the value returns from that *outer* body lambda
// instead of the real function, and silently gets the wrong result rather
// than a compile error (see the `-Wreturn-type` "control reaches end of
// non-void function" warning it produces). Not exercised here since it isn't
// a case this feature claims to support; flagged in DESIGN_NOTES.md for
// future investigation.

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
