#include <cstddef>
#include <span>
#include "ilp_for.hpp"
#include "../escape.hpp"

__attribute__((noinline))
unsigned sum_range_ilp(std::span<const unsigned> data) {
    escape(data);
    auto result = ILP_REDUCE_RANGE(std::plus<>{}, 0u, auto&& val, data, 4) {
        return val;
    } ILP_END_REDUCE;
    escape(result);
    return result;
}
