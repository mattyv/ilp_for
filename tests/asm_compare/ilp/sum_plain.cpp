#include <cstddef>
#include "ilp_for.hpp"

__attribute__((noinline))
unsigned sum_plain_ilp(unsigned n) {
    unsigned sum = 0;
    ILP_FOR_SIMPLE(i, 0u, n, 4) {
        sum += i;
    } ILP_END;
    return sum;
}
