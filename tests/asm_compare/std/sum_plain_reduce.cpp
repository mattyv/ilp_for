#include <cstddef>
#include <numeric>
#include <ranges>

__attribute__((noinline))
unsigned sum_plain_reduce_std(unsigned n) {
    auto range = std::views::iota(0u, n);
    return std::reduce(range.begin(), range.end(), 0u);
}
