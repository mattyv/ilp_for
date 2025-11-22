#include <cstddef>
#include <functional>
#include "ilp_for.hpp"

__attribute__((noinline))
int sum_negative_ilp(int start, int end) {
    return ILP_REDUCE_SIMPLE(std::plus<>{}, 0, i, start, end, 4) {
        return i;
    } ILP_END_REDUCE;
}
