#include <cstddef>
#include "../escape.hpp"

__attribute__((noinline))
unsigned sum_plain_simple(unsigned n) {
    escape(n);
    unsigned sum = 0;
    for (unsigned i = 0; i < n; ++i) {
        sum += i;
    }
    escape(sum);
    return sum;
}
