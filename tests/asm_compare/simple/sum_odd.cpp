#include <cstddef>

__attribute__((noinline))
unsigned sum_odd_simple(unsigned n) {
    unsigned sum = 0;
    for (unsigned i = 0; i < n; ++i) {
        if (i % 2 == 1) {
            sum += i;
        }
    }
    return sum;
}
