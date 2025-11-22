#include <cstddef>

__attribute__((noinline))
unsigned sum_plain_handrolled(unsigned n) {
    unsigned sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
    unsigned i = 0;
    for (; i + 4 <= n; i += 4) {
        sum0 += i;
        sum1 += i + 1;
        sum2 += i + 2;
        sum3 += i + 3;
    }
    for (; i < n; ++i) {
        sum0 += i;
    }
    return sum0 + sum1 + sum2 + sum3;
}
