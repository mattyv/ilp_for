#include <cstddef>
#include <functional>
#include "ilp_for.hpp"

__attribute__((noinline))
unsigned sum_odd_ilp(unsigned n) {
    return ILP_REDUCE(std::plus<>{}, 0u, auto i, 0u, n, 4) {
        return (i % 2 != 0) ? i : 0u;
    } ILP_END_REDUCE;
}
