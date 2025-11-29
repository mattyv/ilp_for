// Example: Find first element matching a predicate
// Demonstrates early-exit search with ILP

#include "../ilp_for.hpp"
#include <vector>
#include <optional>
#include <iostream>

// Find first value greater than threshold
std::optional<size_t> find_first_above(const std::vector<int>& data, int threshold) {
    size_t idx = ILP_FIND(auto i, 0uz, data.size(), 4) {
        return data[i] > threshold;
    } ILP_END;
    return (idx != data.size()) ? std::optional(idx) : std::nullopt;
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
