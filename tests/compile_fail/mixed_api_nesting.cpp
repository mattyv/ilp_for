// COMPILE_FAIL: ilp_for loop bodies must return void
#include "../../ilp_for.hpp"
#include <vector>

int find_match(const std::vector<std::vector<int>>& rows, int target) {
    auto outer = ilp::for_loop<2>(std::size_t{0}, rows.size(), [&](auto r, auto& outer_ctrl) {
        (void)outer_ctrl;
        ILP_FOR(auto c, std::size_t{0}, rows[r].size(), 4) {
            if (rows[r][c] == target)
                ILP_RETURN(static_cast<int>(r));
        }
        ILP_END_RETURN;
    });
    return outer ? *std::move(outer) : -1;
}

int main() {
    std::vector<std::vector<int>> rows = {{1, 2}, {3, 4}};
    return find_match(rows, 4);
}
