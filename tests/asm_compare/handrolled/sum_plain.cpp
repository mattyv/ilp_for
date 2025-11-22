#include <cstddef>

__attribute__((noinline))
unsigned sum_plain_handrolled(unsigned n) {
    unsigned sum = 0;
    unsigned i = 0;
    for (; i + 4 <= n; i += 4) {
        sum += i;
        sum += i + 1;
        sum += i + 2;
        sum += i + 3;
    }
    for (; i < n; ++i) {
        sum += i;
    }
    return sum;
}
