#include "../escape.hpp"
#include <cstddef>
#include <span>

__attribute__((noinline)) unsigned sum_range_simple(std::span<const unsigned> arr) {
    escape(arr);
    unsigned sum = 0;
    for (auto val : arr) {
        sum += val;
    }
    escape(sum);
    return sum;
}
