#include <cstddef>
#include "ilp_for.hpp"
#include "../escape.hpp"

__attribute__((noinline))
unsigned sum_plain_ilp(unsigned n) {
    escape(n);
    auto result = ILP_REDUCE(std::plus<>{}, 0u, auto i, 0u, n, 4) {
        return i;
    } ILP_END_REDUCE;
    escape(result);
    return result;
}
