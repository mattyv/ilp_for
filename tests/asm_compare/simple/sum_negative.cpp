#include <cstddef>

__attribute__((noinline)) unsigned sum_negative_simple(unsigned n) {
    unsigned sum = 0;
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        sum += static_cast<unsigned>(i);
    }
    return sum;
}
