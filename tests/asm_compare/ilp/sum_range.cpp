#if !defined(ILP_MODE_SIMPLE)
#include "../escape.hpp"
#include "ilp_for.hpp"
#include <cstddef>
#include <span>

__attribute__((noinline)) unsigned sum_range_ilp(std::span<const unsigned> data) {
    escape(data);
    auto result = ilp::reduce_range<4>(data, 0u, std::plus<>{}, [](auto&& val) { return val; });
    escape(result);
    return result;
}

#endif // !ILP_MODE_SIMPLE
