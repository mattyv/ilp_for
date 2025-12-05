#if !defined(ILP_MODE_SUPER_SIMPLE)
#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)
#include <cstddef>
#include <optional>
#include <span>
#include "ilp_for.hpp"

__attribute__((noinline))
std::optional<std::size_t> find_value_ilp(std::span<const int> data, [[maybe_unused]] int target) {
    ILP_FOR(auto i, 0uz, data.size(), 4) {
        if (data[i] == target) ILP_RETURN(i);
    } ILP_END_RETURN;
    return std::nullopt;
}
#endif

#endif // !ILP_MODE_SUPER_SIMPLE
