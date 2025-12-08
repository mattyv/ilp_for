// counting with reduce

#include "../ilp_for.hpp"
#include <iostream>
#include <vector>

size_t count_value(const std::vector<int>& data, int target) {
    return ilp::reduce_range_auto<ilp::LoopType::Sum>(data, 0uz, std::plus<>{},
                                                      [&](auto&& val) { return val == target ? 1uz : 0uz; });
}

int main() {
    std::vector<int> data = {1, 2, 3, 2, 4, 2, 5, 2, 6};

    size_t count = count_value(data, 2);
    std::cout << "Count of 2: " << count << "\n";
}
