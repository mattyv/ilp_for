#if !defined(ILP_MODE_SUPER_SIMPLE)
#include <cstddef>
#include <span>
#include "ilp_for.hpp"
#include "../escape.hpp"

__attribute__((noinline))
unsigned sum_range_ilp(std::span<const unsigned> data) {
    escape(data);
    auto result = ilp::reduce_range<4>(data, 0u, std::plus<>{}, [](auto&& val) {
        return val;
    });
    escape(result);
    return result;
}

#endif // !ILP_MODE_SUPER_SIMPLE
