#include <cstddef>

__attribute__((noinline)) unsigned sum_with_break_handrolled(unsigned n, unsigned stop_at) {
    unsigned sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
    unsigned i = 0;
    bool stopped = false;
    for (; i + 4 <= n && !stopped; i += 4) {
        // Check all elements in block and add only if < stop_at
        if (i < stop_at)
            sum0 += i;
        else {
            stopped = true;
        }
        if (i + 1 < stop_at)
            sum1 += i + 1;
        else {
            stopped = true;
        }
        if (i + 2 < stop_at)
            sum2 += i + 2;
        else {
            stopped = true;
        }
        if (i + 3 < stop_at)
            sum3 += i + 3;
        else {
            stopped = true;
        }
    }
    for (; i < n && !stopped; ++i) {
        if (i >= stop_at)
            break;
        sum0 += i;
    }
    return sum0 + sum1 + sum2 + sum3;
}
