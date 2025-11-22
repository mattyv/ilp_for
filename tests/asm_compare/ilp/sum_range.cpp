#include <cstddef>
#include <span>
#include "ilp_for.hpp"

__attribute__((noinline))
int sum_range_ilp(std::span<const int> data) {
    int sum = 0;
    ILP_FOR_RANGE(val, data, 4) {
        sum += val;
    } ILP_END;
    return sum;
}
