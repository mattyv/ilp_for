#if !defined(ILP_MODE_SIMPLE)
#include "ilp_for.hpp"
#include <cstddef>
#include <functional>

__attribute__((noinline)) unsigned sum_odd_ilp(unsigned n) {
    return ilp::reduce<4>(0u, n, 0u, std::plus<>{}, [](auto i) { return (i % 2 != 0) ? i : 0u; });
}

#endif // !ILP_MODE_SIMPLE
