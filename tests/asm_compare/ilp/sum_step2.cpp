#include <cstddef>
#include "ilp_for.hpp"

__attribute__((noinline))
unsigned sum_step2_ilp(unsigned n) {
    return ILP_REDUCE_STEP_SUM(i, 0u, n, 2u, 4) {
        return i;
    } ILP_END_REDUCE;
}
