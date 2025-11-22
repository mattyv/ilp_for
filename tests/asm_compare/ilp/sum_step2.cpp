#include <cstddef>
#include "ilp_for.hpp"

__attribute__((noinline))
unsigned sum_step2_ilp(unsigned n) {
    unsigned sum = 0;
    ILP_FOR_STEP(i, 0u, n, 2u, 4) {
        sum += i;
    } ILP_END;
    return sum;
}
