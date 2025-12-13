// parallel min/max in single pass

#include "../ilp_for.hpp"
#include <iostream>
#include <limits>
#include <vector>

struct MinMax {
    int min = std::numeric_limits<int>::max();
    int max = std::numeric_limits<int>::min();
};

MinMax find_min_max(const std::vector<int>& data) {
    MinMax init{std::numeric_limits<int>::max(), std::numeric_limits<int>::min()};

    auto op = [](MinMax a, MinMax b) { return MinMax{std::min(a.min, b.min), std::max(a.max, b.max)}; };

    return ilp::transform_reduce_auto<ilp::LoopType::MinMax>(data, init, op, [&](auto&& val) { return MinMax{val, val}; });
}

int main() {
    std::vector<int> data = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5};

    auto [min, max] = find_min_max(data);
    std::cout << "Min: " << min << ", Max: " << max << "\n";
}
