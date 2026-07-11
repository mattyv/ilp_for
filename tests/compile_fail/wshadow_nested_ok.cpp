// COMPILE_OK
// EXTRA_FLAGS: -Wshadow -Werror=shadow
// Nested ILP_FOR necessarily reuses fixed names (ilp_detail_ret/ilp_detail_ctx/
// ilp_detail_ctrl) at each nesting level - which is what makes propagate_return's
// unqualified lookup work (see ilp_for.hpp's "Nested ILP_RETURN propagation"
// note) - but would otherwise trip -Wshadow once per nesting level, purely from
// the macro expansion (DESIGN_NOTES.md item 4). The library marks its own macro
// definitions as a system header specifically so this compiles clean even under
// -Werror=shadow. Must compile with zero warnings.
#include "../../ilp_for.hpp"
#include <vector>

int find_first_match(const std::vector<std::vector<int>>& rows, int target) {
    ILP_FOR(auto r, std::size_t{0}, rows.size(), 2) {
        ILP_FOR(auto c, std::size_t{0}, rows[r].size(), 4) {
            if (rows[r][c] == target)
                ILP_RETURN(static_cast<int>(r * 100 + c));
        }
        ILP_END_RETURN;
    }
    ILP_END_RETURN;
    return -1;
}

int main() {
    std::vector<std::vector<int>> rows = {{1, 2, 3}, {4, 5, 6}};
    return find_first_match(rows, 5) == 101 ? 0 : 1;
}
