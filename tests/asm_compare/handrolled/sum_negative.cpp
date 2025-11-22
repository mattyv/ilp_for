#include <cstddef>

__attribute__((noinline))
int sum_negative_handrolled(int start, int end) {
    int sum = 0;
    int i = start;
    for (; i + 4 <= end; i += 4) {
        sum += i;
        sum += i + 1;
        sum += i + 2;
        sum += i + 3;
    }
    for (; i < end; ++i) {
        sum += i;
    }
    return sum;
}
