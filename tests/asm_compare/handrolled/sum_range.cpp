#include <cstddef>
#include <span>

__attribute__((noinline))
int sum_range_handrolled(std::span<const int> data) {
    int sum = 0;
    std::size_t n = data.size();
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        sum += data[i];
        sum += data[i + 1];
        sum += data[i + 2];
        sum += data[i + 3];
    }
    for (; i < n; ++i) {
        sum += data[i];
    }
    return sum;
}
