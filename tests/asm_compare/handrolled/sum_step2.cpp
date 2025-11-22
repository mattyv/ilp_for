#include <cstddef>

__attribute__((noinline))
unsigned sum_step2_handrolled(unsigned n) {
    unsigned sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
    unsigned i = 0;
    constexpr unsigned step = 2;
    for (; i + step * 4 <= n; i += step * 4) {
        sum0 += i;
        sum1 += i + step;
        sum2 += i + step * 2;
        sum3 += i + step * 3;
    }
    for (; i < n; i += step) {
        sum0 += i;
    }
    return sum0 + sum1 + sum2 + sum3;
}
