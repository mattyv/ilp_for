#include <cstddef>
#include "ilp_for.hpp"

__attribute__((noinline))
unsigned sum_plain_ilp(unsigned n) {
    return ILP_REDUCE_SUM(i, 0u, n, 4) {
        return i;
    } ILP_END_REDUCE;
}
