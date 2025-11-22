#include <cstddef>

__attribute__((noinline))
unsigned sum_step2_handrolled(unsigned n) {
    unsigned sum = 0;
    unsigned i = 0;
    constexpr unsigned step = 2;
    for (; i + step * 4 <= n; i += step * 4) {
        sum += i;
        sum += i + step;
        sum += i + step * 2;
        sum += i + step * 3;
    }
    for (; i < n; i += step) {
        sum += i;
    }
    return sum;
}
