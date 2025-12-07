#include <cstddef>

__attribute__((noinline)) unsigned sum_odd_handrolled(unsigned n) {
    unsigned sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
    unsigned i = 0;
    for (; i + 4 <= n; i += 4) {
        if (i % 2 != 0)
            sum0 += i;
        if ((i + 1) % 2 != 0)
            sum1 += i + 1;
        if ((i + 2) % 2 != 0)
            sum2 += i + 2;
        if ((i + 3) % 2 != 0)
            sum3 += i + 3;
    }
    for (; i < n; ++i) {
        if (i % 2 == 0)
            continue;
        sum0 += i;
    }
    return sum0 + sum1 + sum2 + sum3;
}
