#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)
#include <cstddef>
#include <functional>
#include "ilp_for.hpp"

__attribute__((noinline))
unsigned sum_with_break_ilp(unsigned n, unsigned stop_at) {
    return ILP_REDUCE(std::plus<>{}, 0u, auto i, 0u, n, 4) {
        if (i >= stop_at) {
            _ilp_ctrl.break_loop();
            return 0u;  // Return neutral element when breaking
        }
        return i;
    } ILP_END_REDUCE;
}
#endif
