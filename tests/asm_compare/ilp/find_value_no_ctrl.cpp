#if !defined(ILP_MODE_SIMPLE) && !defined(ILP_MODE_PRAGMA)
#include <cstddef>
#include <optional>
#include <span>
#include "ilp_for.hpp"

__attribute__((noinline))
std::optional<std::size_t> find_value_no_ctrl_ilp(std::span<const int> data, int target) {
    ILP_FOR_RET(std::size_t, i, 0uz, data.size(), 4) {
        if (data[i] == target) ILP_RETURN(i);
    } ILP_END_RET;
    return std::nullopt;
}
#endif
