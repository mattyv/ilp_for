#include <cstddef>
#include "ilp_for.hpp"

__attribute__((noinline))
int sum_backward_ilp(int start, int end) {
    int sum = 0;
    ILP_FOR_STEP(i, start, end, -1, 4) {
        sum += i;
    } ILP_END;
    return sum;
}
