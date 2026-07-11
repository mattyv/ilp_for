#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <vector>

// Coverage for nested ILP_RETURN propagation (see docs/NESTED_RETURN_PLAN.md).
// ILP_RETURN returns from the enclosing C++ function at any nesting depth,
// provided every enclosing ILP_FOR on the path out is closed with
// ILP_END_RETURN. The macro layer is unconditional (ILP_MODE_SIMPLE only
// flips ilp::default_mode - see mode.hpp), so this mechanism and every case
// below hold identically in both build modes. The negative case (an enclosing
// loop closed with plain ILP_END, which cannot carry a value out) lives in
// tests/compile_fail/nested_return_in_end_loop.cpp.
//
// Type caveat (applies in BOTH modes): propagation of an *untyped* inner
// ILP_RETURN into an outer typed (ILP_FOR_T) or SBO-typed context recovers
// the value via the same type-erased SBO pun documented in DESIGN_NOTES.md
// item 3 - the bytes ILP_RETURN(x) stored are reinterpreted as the outer
// type, not value-converted. Results are therefore only well-defined when the
// propagated expression's type already matches the type it's being read back
// as at every hop (true of every case below - see the per-test-case notes for
// the exact type at each level). A mismatched-width example (untyped
// ILP_RETURN(int) propagating into an ILP_FOR_T(long) outer) is analyzed, not
// asserted, in item 3; debug builds abort on it, naming both types.

TEST_CASE("2-level nesting: inner ILP_RETURN escapes the outer loop", "[nested_return]") {
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
    // row 1, col 2 -> 1*100+2 = 102
    REQUIRE(find_first_match(rows, 6) == 102);
    // row 0, col 0 -> 0
    REQUIRE(find_first_match(rows, 1) == 0);
    // row 2, col 2 -> 202
    REQUIRE(find_first_match(rows, 9) == 202);
    REQUIRE(find_first_match(rows, 999) == -1); // not found: falls through both loops
}

TEST_CASE("3-level nesting: inner ILP_RETURN escapes two outer loops", "[nested_return]") {
    auto find_3d = [](const std::vector<std::vector<std::vector<int>>>& cube, int target) -> int {
        ILP_FOR(auto x, std::size_t{0}, cube.size(), 2) {
            ILP_FOR(auto y, std::size_t{0}, cube[x].size(), 2) {
                ILP_FOR(auto z, std::size_t{0}, cube[x][y].size(), 4) {
                    if (cube[x][y][z] == target)
                        ILP_RETURN(static_cast<int>(x * 10000 + y * 100 + z));
                }
                ILP_END_RETURN;
            }
            ILP_END_RETURN;
        }
        ILP_END_RETURN;
        return -1;
    };

    std::vector<std::vector<std::vector<int>>> cube = {{{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}};
    REQUIRE(find_3d(cube, 7) == 10100); // x=1, y=0, z=1
    REQUIRE(find_3d(cube, 1) == 0);     // x=0, y=0, z=0
    REQUIRE(find_3d(cube, 99) == -1);
}

TEST_CASE("Typed inner inside typed outer (same large type)", "[nested_return][typed]") {
    struct Big {
        int a, b, c;
        double d;
    };

    auto find_big = [](const std::vector<std::vector<int>>& rows, int target) -> Big {
        ILP_FOR_T(Big, auto r, std::size_t{0}, rows.size(), 2) {
            ILP_FOR_T(Big, auto c, std::size_t{0}, rows[r].size(), 4) {
                if (rows[r][c] == target)
                    ILP_RETURN((Big{static_cast<int>(r), static_cast<int>(c), 7, 1.5}));
            }
            ILP_END_RETURN;
        }
        ILP_END_RETURN;
        return Big{-1, -1, -1, 0.0};
    };

    std::vector<std::vector<int>> rows = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    Big found = find_big(rows, 8);
    REQUIRE(found.a == 2);
    REQUIRE(found.b == 1);
    REQUIRE(found.c == 7);
    REQUIRE(found.d == 1.5);

    Big missing = find_big(rows, 999);
    REQUIRE(missing.a == -1);
}

TEST_CASE("Untyped inner inside typed outer", "[nested_return][typed]") {
    // Exercises the untyped-inner -> typed-outer propagation hop from
    // propagate_return, the one closest to the type-pun risk described at the
    // top of this file: the inner ILP_RETURN(static_cast<int>(...)) result is
    // recovered via extract<int>() into the outer ILP_FOR_T(int) - safe here
    // because the propagated expression's type (int) already matches the
    // outer's declared type (int). A narrower/wider mismatch would reinterpret
    // rather than convert; see DESIGN_NOTES.md item 3.
    auto find_mixed = [](const std::vector<std::vector<int>>& rows, int target) -> int {
        ILP_FOR_T(int, auto r, std::size_t{0}, rows.size(), 2) {
            ILP_FOR(auto c, std::size_t{0}, rows[r].size(), 4) {
                if (rows[r][c] == target)
                    ILP_RETURN(static_cast<int>(r * 100 + c));
            }
            ILP_END_RETURN;
        }
        ILP_END_RETURN;
        return -1;
    };

    std::vector<std::vector<int>> rows = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    REQUIRE(find_mixed(rows, 5) == 101);
    REQUIRE(find_mixed(rows, 999) == -1);
}

TEST_CASE("Control flow around a nested break-only loop is unaffected", "[nested_return]") {
    // The outer loop's own ILP_CONTINUE/ILP_BREAK still work normally around an
    // inner ILP_END (break-only) loop - propagation only concerns ILP_RETURN.
    auto sum_rows_until = [](const std::vector<std::vector<int>>& rows, int stop_row_sum) -> int {
        int total = 0;
        ILP_FOR(auto r, std::size_t{0}, rows.size(), 2) {
            if (rows[r].empty())
                ILP_CONTINUE;
            int row_sum = 0;
            ILP_FOR(auto c, std::size_t{0}, rows[r].size(), 4) {
                row_sum += rows[r][c];
            }
            ILP_END;
            if (row_sum >= stop_row_sum)
                ILP_BREAK;
            total += row_sum;
        }
        ILP_END;
        return total;
    };

    std::vector<std::vector<int>> rows = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    // row0 sum=6 < 15, added (total=6); row1 sum=15 >= 15, break before adding
    REQUIRE(sum_rows_until(rows, 15) == 6);
}

TEST_CASE("Nested propagation produces identical results under ILP_MODE_SIMPLE (matching types)",
          "[nested_return][mode_parity]") {
    // This test file has no #if guards around ILP_END_RETURN nesting - it is
    // compiled and run under both the default and ILP_MODE_SIMPLE builds via
    // test_all_modes.sh, and every REQUIRE above must hold in both. This case
    // makes that mode-parity claim explicit and self-contained. It holds here
    // because the propagated type (int) is uniform at every level - see the
    // file-level comment above for the type-pun caveat when that's not true.
    auto find_first_match = [](const std::vector<std::vector<int>>& rows, int target) -> int {
        ILP_FOR(auto r, std::size_t{0}, rows.size(), 2) {
            ILP_FOR(auto c, std::size_t{0}, rows[r].size(), 4) {
                if (rows[r][c] == target)
                    ILP_RETURN(static_cast<int>(r * 100 + c));
            }
            ILP_END_RETURN;
        }
        ILP_END_RETURN;
        return -1;
    };

    std::vector<std::vector<int>> rows = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    REQUIRE(find_first_match(rows, 6) == 102);
    REQUIRE(find_first_match(rows, 999) == -1);
}
