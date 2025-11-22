#include <cstddef>

__attribute__((noinline))
unsigned sum_with_break_handrolled(unsigned n, unsigned limit) {
    unsigned sum = 0;
    unsigned i = 0;
    bool should_break = false;
    for (; i + 4 <= n && !should_break; i += 4) {
        sum += i;
        if (sum > limit) { should_break = true; break; }
        sum += i + 1;
        if (sum > limit) { should_break = true; break; }
        sum += i + 2;
        if (sum > limit) { should_break = true; break; }
        sum += i + 3;
        if (sum > limit) { should_break = true; break; }
    }
    for (; i < n && !should_break; ++i) {
        sum += i;
        if (sum > limit) break;
    }
    return sum;
}
