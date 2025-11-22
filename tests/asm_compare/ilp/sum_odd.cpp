#include <cstddef>
#include "ilp_for.hpp"

__attribute__((noinline))
unsigned sum_odd_ilp(unsigned n) {
    unsigned sum = 0;
    ILP_FOR(i, 0u, n, 4) {
        if (i % 2 == 0) ILP_CONTINUE;
        sum += i;
    } ILP_END;
    return sum;
}
