#include <cstddef>
#include "ilp_for.hpp"

__attribute__((noinline))
unsigned sum_with_break_ilp(unsigned n, unsigned limit) {
    unsigned sum = 0;
    ILP_FOR(i, 0u, n, 4) {
        sum += i;
        if (sum > limit) ILP_BREAK;
    } ILP_END;
    return sum;
}
