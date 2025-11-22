#include <cstddef>
#include <numeric>
#include <ranges>
#include "../escape.hpp"

__attribute__((noinline))
unsigned sum_plain_std(unsigned n) {
    escape(n);
    auto range = std::views::iota(0u, n);
    auto result = std::accumulate(range.begin(), range.end(), 0u);
    escape(result);
    return result;
}
