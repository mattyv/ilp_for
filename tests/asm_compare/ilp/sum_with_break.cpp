#if !defined(ILP_MODE_SIMPLE)
#if !defined(ILP_MODE_SIMPLE)
#include <cstddef>
#include <functional>
#include <optional>
#include "ilp_for.hpp"

__attribute__((noinline))
unsigned sum_with_break_ilp(unsigned n, unsigned stop_at) {
    return ilp::reduce<4>(0u, n, 0u, std::plus<>{},
        [&](auto i) -> std::optional<unsigned> {
            if (i >= stop_at) return std::nullopt;
            return i;
        });
}
#endif

#endif // !ILP_MODE_SIMPLE
