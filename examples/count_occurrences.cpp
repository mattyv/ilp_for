// Example: Count occurrences of a value
// Demonstrates simple reduction without early exit

#include "../ilp_for.hpp"
#include <vector>
#include <iostream>

size_t count_value(const std::vector<int>& data, int target) {
    return ilp::reduce_range_auto(data, 0uz, std::plus<>{}, [&](auto&& val) {
        return val == target ? 1uz : 0uz;
    });
}

int main() {
    std::vector<int> data = {1, 2, 3, 2, 4, 2, 5, 2, 6};

    size_t count = count_value(data, 2);
    std::cout << "Count of 2: " << count << "\n";
}
