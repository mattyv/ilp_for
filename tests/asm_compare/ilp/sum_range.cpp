#include <cstddef>
#include <span>
#include "ilp_for.hpp"

__attribute__((noinline))
int sum_range_ilp(std::span<const int> data) {
    return ILP_REDUCE_RANGE_SUM(val, data, 4) {
        return val;
    } ILP_END_REDUCE;
}
