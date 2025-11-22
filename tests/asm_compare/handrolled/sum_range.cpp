#include <cstddef>
#include <span>

__attribute__((noinline))
int sum_range_handrolled(std::span<const int> data) {
    int sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
    std::size_t n = data.size();
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        sum0 += data[i];
        sum1 += data[i + 1];
        sum2 += data[i + 2];
        sum3 += data[i + 3];
    }
    for (; i < n; ++i) {
        sum0 += data[i];
    }
    return sum0 + sum1 + sum2 + sum3;
}
