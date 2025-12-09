// manual unroll factor

#include "../ilp_for.hpp"
#include <array>
#include <iostream>
#include <vector>

constexpr auto N_SUM_DOUBLE = ilp::optimal_N<ilp::LoopType::Sum, double>;
constexpr auto N_SEARCH_INT = ilp::optimal_N<ilp::LoopType::Search, int>;

template<size_t Size>
int sum_small_array(const std::array<int, Size>& arr) {
    int sum = 0;
    ILP_FOR(auto i, 0uz, Size, 2) {
        sum += arr[i];
    }
    ILP_END;
    return sum;
}

// Known hot path with profiled optimal N
// After benchmarking, N=8 was determined optimal for this specific workload
double dot_product_tuned(const double* a, const double* b, size_t n) {
    return ilp::reduce<8>(0uz, n, 0.0, std::plus<>{}, [&](auto i) { return a[i] * b[i]; });
}

// Compare AUTO vs manual for demonstration
void compare_approaches(const std::vector<int>& data) {
    // AUTO: lets the library choose based on CPU profile and element size
    auto sum_auto = ilp::reduce_range_auto<ilp::LoopType::Sum>(data, 0, std::plus<>{}, [&](auto&& val) { return val; });

    // Manual N=4: explicit control
    auto sum_manual = ilp::reduce_range<4>(data, 0, std::plus<>{}, [&](auto&& val) { return val; });

    // Manual N=16: aggressive unrolling for large data
    auto sum_aggressive = ilp::reduce_range<16>(data, 0, std::plus<>{}, [&](auto&& val) { return val; });

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
    }
    ILP_END;
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
    for (size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<int>(i + 1);
    compare_approaches(data);

    std::cout << "\nRecommended N values from optimal_N:\n";
    std::cout << "  Sum<double>: " << N_SUM_DOUBLE << "\n";
    std::cout << "  Search<int>: " << N_SEARCH_INT << "\n";
}
