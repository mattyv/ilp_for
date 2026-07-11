// RUN_ABORT: stored a value
// Nested-propagation hop pun: an untyped inner ILP_RETURN(int) propagates
// through detail::propagate_return into an ILP_FOR_T(long) outer loop. The
// same SmallStorage::extract check that catches the top-level pun catches
// this automatically, since propagate_return funnels through it too.
#include "../../ilp_for.hpp"
#include <vector>

long f(const std::vector<std::vector<int>>& rows, int t) {
    ILP_FOR_T(long, auto r, std::size_t{0}, rows.size(), 2) {
        ILP_FOR(auto c, std::size_t{0}, rows[r].size(), 4) {
            if (rows[r][c] == t)
                ILP_RETURN(static_cast<int>(c)); // int into long outer
        }
        ILP_END_RETURN;
    }
    ILP_END_RETURN;
    return -1;
}

int main() {
    std::vector<std::vector<int>> rows{{1, 2}, {3, 4}};
    return static_cast<int>(f(rows, 4));
}
