#include <cstddef>

__attribute__((noinline))
unsigned sum_odd_handrolled(unsigned n) {
    unsigned sum = 0;
    unsigned i = 0;
    for (; i + 4 <= n; i += 4) {
        if (i % 2 != 0) sum += i;
        if ((i + 1) % 2 != 0) sum += i + 1;
        if ((i + 2) % 2 != 0) sum += i + 2;
        if ((i + 3) % 2 != 0) sum += i + 3;
    }
    for (; i < n; ++i) {
        if (i % 2 == 0) continue;
        sum += i;
    }
    return sum;
}
