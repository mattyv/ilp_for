// Example: Reduce with early exit
// Demonstrates ILP_REDUCE with ILP_BREAK_RET
//
// Note: ILP_BREAK_RET breaks based on index or external conditions,
// not on the accumulated value (multiple parallel accumulators make
// that impractical). For running-total checks, use a regular loop.

#include "../ilp_for.hpp"
#include <vector>
#include <iostream>

// Sum first N elements only
int sum_first_n(const std::vector<int>& data, size_t n) {
    return ILP_REDUCE(std::plus<>{}, 0, auto i, 0uz, data.size(), 4) {
        if (i >= n) {
            ILP_BREAK_RET(0);  // Stop, contribute 0 for this iteration
        }
        return data[i];
    } ILP_END_REDUCE;
}

// Sum until sentinel value encountered
int sum_until_sentinel(const std::vector<int>& data, int sentinel) {
    return ILP_REDUCE(std::plus<>{}, 0, auto i, 0uz, data.size(), 4) {
        if (data[i] == sentinel) {
            ILP_BREAK_RET(0);  // Stop at sentinel
        }
        return data[i];
    } ILP_END_REDUCE;
}

// Count positive values, stop at first negative
size_t count_positive_until_negative(const std::vector<int>& data) {
    return ILP_REDUCE(std::plus<>{}, 0uz, auto i, 0uz, data.size(), 4) {
        if (data[i] < 0) {
            ILP_BREAK_RET(0uz);  // Negative found, stop
        }
        return data[i] > 0 ? 1uz : 0uz;
    } ILP_END_REDUCE;
}

// Sum with skip and early termination
// Skip zeros, stop at negative
int sum_nonzero_until_negative(const std::vector<int>& data) {
    return ILP_REDUCE(std::plus<>{}, 0, auto i, 0uz, data.size(), 4) {
        if (data[i] < 0) {
            ILP_BREAK_RET(0);  // Error marker, stop
        }
        if (data[i] == 0) {
            return 0;  // Skip zeros (continue)
        }
        return data[i];
    } ILP_END_REDUCE;
}

// Product with early termination on zero
int64_t product_until_zero(const std::vector<int>& data) {
    return ILP_REDUCE(std::multiplies<>{}, 1LL, auto i, 0uz, data.size(), 4) {
        if (data[i] == 0) {
            ILP_BREAK_RET(1LL);  // Zero found, product is 0 anyway
        }
        return static_cast<int64_t>(data[i]);
    } ILP_END_REDUCE;
}

int main() {
    // Sum first N elements
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::cout << "Sum of first 5: " << sum_first_n(data, 5) << "\n";
    std::cout << "Sum of first 3: " << sum_first_n(data, 3) << "\n\n";

    // Sum until sentinel
    std::vector<int> with_sentinel = {10, 20, 30, -1, 40, 50};  // -1 is sentinel
    std::cout << "Sum until -1: " << sum_until_sentinel(with_sentinel, -1) << "\n\n";

    // Count positive until negative
    std::vector<int> mixed = {5, 3, 0, 7, 2, -1, 8, 9};
    std::cout << "Positive count before negative: "
              << count_positive_until_negative(mixed) << "\n\n";

    // Sum nonzero until negative
    std::vector<int> sparse = {1, 0, 2, 0, 0, 3, -1, 4, 5};
    std::cout << "Sum nonzero until negative: "
              << sum_nonzero_until_negative(sparse) << "\n\n";

    // Product with zero termination
    std::vector<int> factors = {2, 3, 4, 0, 5, 6};
    int64_t prod = product_until_zero(factors);
    // Note: result depends on unrolling - may not be exactly 24
    // because parallel accumulators may see 0 at different times
    std::cout << "Product until zero: " << prod << "\n";
}
