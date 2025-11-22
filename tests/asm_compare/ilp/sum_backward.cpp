#include <cstddef>
#include "ilp_for.hpp"

__attribute__((noinline))
int sum_backward_ilp(int start, int end) {
    return ILP_REDUCE_STEP_SUM(i, start, end, -1, 4) {
        return i;
    } ILP_END_REDUCE;
}
