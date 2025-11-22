#include <cstddef>

__attribute__((noinline))
unsigned sum_step2_simple(unsigned n) {
    unsigned sum = 0;
    for (unsigned i = 0; i < n; i += 2) {
        sum += i;
    }
    return sum;
}
