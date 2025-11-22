#include <cstddef>
#include "ilp_for.hpp"

__attribute__((noinline))
int sum_negative_ilp(int start, int end) {
    int sum = 0;
    ILP_FOR(i, start, end, 4) {
        sum += i;
    } ILP_END;
    return sum;
}
