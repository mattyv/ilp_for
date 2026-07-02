// COMPILE_FAIL: Cannot store a loop-result Proxy
// Hand-propagating a nested ilp::for_loop's result via
// ctrl.return_with(*std::move(r)) would store a dangling Proxy (it wraps a
// reference to storage that is about to go out of scope). Must fail to
// compile with a message pointing at the correct alternatives.
#include "../../ilp_for.hpp"
#include <vector>

int find_first_match(const std::vector<std::vector<int>>& rows, int target) {
    auto outer = ilp::for_loop<2>(std::size_t{0}, rows.size(), [&](auto r, auto& outer_ctrl) {
        auto inner = ilp::for_loop<4>(std::size_t{0}, rows[r].size(), [&](auto c, auto& inner_ctrl) {
            if (rows[r][c] == target)
                return inner_ctrl.return_with(static_cast<int>(r * 100 + c));
        });
        if (inner)
            outer_ctrl.return_with(*std::move(inner)); // WRONG: stores a dangling Proxy
    });
    if (outer)
        return *std::move(outer);
    return -1;
}

int main() {
    std::vector<std::vector<int>> rows = {{1, 2, 3}, {4, 5, 6}};
    return find_first_match(rows, 5);
}
