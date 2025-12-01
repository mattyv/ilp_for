#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)
#include <cstddef>
#include <functional>
#include "ilp_for.hpp"

__attribute__((noinline))
unsigned sum_with_break_ilp(unsigned n, unsigned stop_at) {
    return ILP_REDUCE(std::plus<>{}, 0u, auto i, 0u, n, 4) {
        if (i >= stop_at) ILP_REDUCE_BREAK;
        ILP_REDUCE_RETURN(i);
    } ILP_END_REDUCE;
}
#endif
