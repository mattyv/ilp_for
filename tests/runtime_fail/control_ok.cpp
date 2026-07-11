// RUN_OK
// Control case: matched types throughout, including a nested propagation hop.
// Must run cleanly (exit 0) - proves the harness isn't passing vacuously, and
// that the type check doesn't false-positive on correct code.
#include "../../ilp_for.hpp"
#include <vector>

int find_idx(const std::vector<int>& v, int t) {
    ILP_FOR(auto i, std::size_t{0}, v.size(), 4) {
        if (v[i] == t) ILP_RETURN(static_cast<int>(i));
    } ILP_END_RETURN;
    return -1;
}

int nested(const std::vector<std::vector<int>>& rows, int t) {
    ILP_FOR(auto r, std::size_t{0}, rows.size(), 2) {
        ILP_FOR(auto c, std::size_t{0}, rows[r].size(), 4) {
            if (rows[r][c] == t) ILP_RETURN(static_cast<int>(r * 100 + c));
        }
        ILP_END_RETURN;
    }
    ILP_END_RETURN;
    return -1;
}

int main() {
    std::vector<int> v{1, 2, 3, 42, 5};
    std::vector<std::vector<int>> rows{{1, 2, 3}, {4, 5, 6}};
    if (find_idx(v, 42) != 3)
        return 1;
    if (nested(rows, 6) != 102)
        return 2;
    return 0;
}
