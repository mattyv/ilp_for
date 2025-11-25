// Example: Find first element matching a predicate
// Demonstrates early-exit search with ILP

#include "../ilp_for.hpp"
#include <vector>
#include <optional>
#include <iostream>

// Find first value greater than threshold
std::optional<size_t> find_first_above(const std::vector<int>& data, int threshold) {
    return ILP_FOR_RET_SIMPLE_AUTO(auto i, 0uz, data.size()) {
        return data[i] > threshold;
    } ILP_END;
}

int main() {
    std::vector<int> data(10'000'000, 42);
    data[7'500'000] = 100;  // Target near end

    auto idx = find_first_above(data, 50);

    if (idx) {
        std::cout << "Found at index " << *idx << ": " << data[*idx] << "\n";
    } else {
        std::cout << "Not found\n";
    }
}
