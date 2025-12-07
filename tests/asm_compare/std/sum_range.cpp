#include "../escape.hpp"
#include <cstddef>
#include <numeric>
#include <span>

__attribute__((noinline)) unsigned sum_range_std(std::span<const unsigned> arr) {
    escape(arr);
    auto result = std::accumulate(arr.begin(), arr.end(), 0u);
    escape(result);
    return result;
}
