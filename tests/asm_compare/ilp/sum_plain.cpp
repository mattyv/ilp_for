#if !defined(ILP_MODE_SIMPLE)
#include "../escape.hpp"
#include "ilp_for.hpp"
#include <cstddef>

__attribute__((noinline)) unsigned sum_plain_ilp(unsigned n) {
    escape(n);
    auto result = ilp::reduce<4>(0u, n, 0u, std::plus<>{}, [](auto i) { return i; });
    escape(result);
    return result;
}

#endif // !ILP_MODE_SIMPLE
