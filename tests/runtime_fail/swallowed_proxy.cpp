// RUN_ABORT: swallowed return value
// DESIGN_NOTES.md item 5: a macro loop (ILP_FOR/.../ILP_END_RETURN) nested
// inside a function-API (ilp::for_loop) lambda body cannot see that lambda's
// ctrl - it has whatever name the caller chose, not the macro's fixed
// ilp_detail_ctrl - so the macro's ILP_END_RETURN treats itself as top-level
// and returns a Proxy that the enclosing lambda then discards without
// converting. Every legitimate Proxy use converts it exactly once; one
// destroyed unconverted is the signature this file's abort detects.
#include "../../ilp_for.hpp"
#include <vector>

int find_always_hits(const std::vector<std::vector<int>>& rows) {
    auto outer = ilp::for_loop<2>(std::size_t{0}, rows.size(), [&](auto r, auto& outer_ctrl) {
        (void)outer_ctrl;
        ILP_FOR(auto c, std::size_t{0}, rows[r].size(), 4) {
            if (rows[r][c] == 0)
                ILP_RETURN(static_cast<int>(r)); // WRONG: nested macro loop inside a for_loop lambda
        }
        ILP_END_RETURN;
    });
    if (outer)
        return *std::move(outer);
    return -1;
}

int main() {
    // Every row contains a 0, so the inner search always matches - this avoids
    // the separate (pre-existing, unrelated) UB of falling off the end of the
    // outer lambda on a non-matching iteration, isolating just the Proxy check.
    std::vector<std::vector<int>> rows = {{0, 1}, {0, 2}, {0, 3}};
    return find_always_hits(rows);
}
