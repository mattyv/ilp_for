// Example: Manual unroll factor specification
// Demonstrates when and why to use explicit N instead of AUTO

#include "../ilp_for.hpp"
#include <vector>
#include <array>
#include <iostream>

// Use optimal_N to query the recommended value for your use case
constexpr auto N_SUM_DOUBLE = ilp::optimal_N<ilp::LoopType::Sum, sizeof(double)>;
constexpr auto N_SEARCH_INT = ilp::optimal_N<ilp::LoopType::Search, sizeof(int)>;

// Small fixed-size array: use small N to avoid overhead
template<size_t Size>
int sum_small_array(const std::array<int, Size>& arr) {
    // For small arrays, N=2 reduces loop overhead while maintaining ILP
    int sum = 0;
    ILP_FOR(auto i, 0uz, Size, 2) {
        sum += arr[i];
    } ILP_END;
    return sum;
}

// Known hot path with profiled optimal N
// After benchmarking, N=8 was determined optimal for this specific workload
double dot_product_tuned(const double* a, const double* b, size_t n) {
    return ILP_REDUCE(std::plus<>{}, 0.0, auto i, 0uz, n, 8) {
        return a[i] * b[i];
    } ILP_END_REDUCE;
}

// Compare AUTO vs manual for demonstration
void compare_approaches(const std::vector<int>& data) {
    // AUTO: lets the library choose based on CPU profile and element size
    auto sum_auto = ILP_REDUCE_RANGE_SUM_AUTO(auto&& val, data) {
        return val;
    } ILP_END_REDUCE;

    // Manual N=4: explicit control
    auto sum_manual = ILP_REDUCE_RANGE_SUM(auto&& val, data, 4) {
        return val;
    } ILP_END_REDUCE;

    // Manual N=16: aggressive unrolling for large data
    auto sum_aggressive = ILP_REDUCE_RANGE_SUM(auto&& val, data, 16) {
        return val;
    } ILP_END_REDUCE;

    std::cout << "AUTO (N=" << N_SUM_DOUBLE << " for double): " << sum_auto << "\n";
    std::cout << "Manual N=4: " << sum_manual << "\n";
    std::cout << "Manual N=16: " << sum_aggressive << "\n";
}

// Memory-bound operation: smaller N reduces register pressure
void process_large_structs(std::vector<std::array<double, 8>>& data) {
    // Large struct = memory bound, N=2 is often optimal
    ILP_FOR(auto i, 0uz, data.size(), 2) {
        for (auto& v : data[i]) {
            v *= 2.0;
        }
    } ILP_END;
}

int main() {
    // Small array optimization
    std::array<int, 8> small = {1, 2, 3, 4, 5, 6, 7, 8};
    std::cout << "Small array sum: " << sum_small_array(small) << "\n\n";

    // Tuned dot product
    std::vector<double> a(1000, 1.5);
    std::vector<double> b(1000, 2.0);
    std::cout << "Dot product: " << dot_product_tuned(a.data(), b.data(), 1000) << "\n\n";

    // Compare approaches
    std::vector<int> data(100);
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<int>(i + 1);
    compare_approaches(data);

    std::cout << "\nRecommended N values from optimal_N:\n";
    std::cout << "  Sum<double>: " << N_SUM_DOUBLE << "\n";
    std::cout << "  Search<int>: " << N_SEARCH_INT << "\n";
}
