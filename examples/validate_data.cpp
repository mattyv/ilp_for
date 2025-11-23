// Example: Validate array elements against constraints
// Demonstrates any-of with early exit

#include "../ilp_for.hpp"
#include <vector>
#include <iostream>

// Check if any value is out of valid range [0, 255]
bool has_invalid_byte(const std::vector<int>& data) {
    auto idx = ILP_FOR_RET_SIMPLE_AUTO(i, 0uz, data.size()) {
        return data[i] < 0 || data[i] > 255;
    } ILP_END;

    return idx != data.size();
}

int main() {
    std::vector<int> bytes = {100, 200, 50, 300, 150};  // 300 is invalid

    if (has_invalid_byte(bytes)) {
        std::cout << "Data contains invalid bytes\n";
    } else {
        std::cout << "All bytes valid\n";
    }
}
