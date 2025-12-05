#if !defined(ILP_MODE_SUPER_SIMPLE)
#include <cstddef>
#include <functional>
#include "ilp_for.hpp"

__attribute__((noinline))
int sum_negative_ilp(int start, int end) {
    return ilp::reduce<4>(start, end, 0, std::plus<>{}, [](auto i) {
        return i;
    });
}

#endif // !ILP_MODE_SUPER_SIMPLE
