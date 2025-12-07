// reduce with early exit (nullopt = break)

#include "../ilp_for.hpp"
#include <iostream>
#include <optional>
#include <vector>

int sum_first_n(const std::vector<int>& data, size_t n) {
    return ilp::reduce<4>(size_t{0}, data.size(), 0, std::plus<>{}, [&](auto i) -> std::optional<int> {
        if (i >= n)
            return std::nullopt;
        return data[i];
    });
}

int sum_until_sentinel(const std::vector<int>& data, int sentinel) {
    return ilp::reduce<4>(size_t{0}, data.size(), 0, std::plus<>{}, [&](auto i) -> std::optional<int> {
        if (data[i] == sentinel)
            return std::nullopt;
        return data[i];
    });
}

size_t count_positive_until_negative(const std::vector<int>& data) {
    return ilp::reduce<4>(size_t{0}, data.size(), size_t{0}, std::plus<>{}, [&](auto i) -> std::optional<size_t> {
        if (data[i] < 0)
            return std::nullopt;
        return data[i] > 0 ? size_t{1} : size_t{0};
    });
}

int sum_nonzero_until_negative(const std::vector<int>& data) {
    return ilp::reduce<4>(size_t{0}, data.size(), 0, std::plus<>{}, [&](auto i) -> std::optional<int> {
        if (data[i] < 0)
            return std::nullopt;
        return data[i];
    });
}

int64_t product_until_zero(const std::vector<int>& data) {
    return ilp::reduce<4>(size_t{0}, data.size(), 1LL, std::multiplies<>{}, [&](auto i) -> std::optional<int64_t> {
        if (data[i] == 0)
            return std::nullopt;
        return static_cast<int64_t>(data[i]);
    });
}

int main() {
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::cout << "Sum of first 5: " << sum_first_n(data, 5) << "\n";
    std::cout << "Sum of first 3: " << sum_first_n(data, 3) << "\n\n";

    std::vector<int> with_sentinel = {10, 20, 30, -1, 40, 50};
    std::cout << "Sum until -1: " << sum_until_sentinel(with_sentinel, -1) << "\n\n";

    std::vector<int> mixed = {5, 3, 0, 7, 2, -1, 8, 9};
    std::cout << "Positive count before negative: " << count_positive_until_negative(mixed) << "\n\n";

    std::vector<int> sparse = {1, 0, 2, 0, 0, 3, -1, 4, 5};
    std::cout << "Sum nonzero until negative: " << sum_nonzero_until_negative(sparse) << "\n\n";

    std::vector<int> factors = {2, 3, 4, 0, 5, 6};
    std::cout << "Product until zero: " << product_until_zero(factors) << "\n";
}
