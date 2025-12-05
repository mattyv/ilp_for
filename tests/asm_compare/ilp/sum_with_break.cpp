#if !defined(ILP_MODE_SUPER_SIMPLE)
#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)
#include <cstddef>
#include <functional>
#include "ilp_for.hpp"

__attribute__((noinline))
unsigned sum_with_break_ilp(unsigned n, unsigned stop_at) {
    return ilp::reduce<4>(0u, n, 0u, std::plus<>{}, [&](auto i) {
        if (i >= stop_at) return ilp::reduce_break<unsigned>();
        return ilp::reduce_value(i);
    });
}
#endif

#endif // !ILP_MODE_SUPER_SIMPLE
