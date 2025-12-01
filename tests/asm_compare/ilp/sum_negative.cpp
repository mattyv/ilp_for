#include <cstddef>
#include <functional>
#include "ilp_for.hpp"

__attribute__((noinline))
int sum_negative_ilp(int start, int end) {
    return ILP_REDUCE(std::plus<>{}, 0, auto i, start, end, 4) {
        ILP_REDUCE_RETURN(i);
    } ILP_END_REDUCE;
}
