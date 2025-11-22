#include <cstddef>

__attribute__((noinline))
int sum_negative_handrolled(int start, int end) {
    int sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
    int i = start;
    for (; i + 4 <= end; i += 4) {
        sum0 += i;
        sum1 += i + 1;
        sum2 += i + 2;
        sum3 += i + 3;
    }
    for (; i < end; ++i) {
        sum0 += i;
    }
    return sum0 + sum1 + sum2 + sum3;
}
