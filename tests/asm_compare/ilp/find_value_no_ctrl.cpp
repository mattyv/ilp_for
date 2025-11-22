#include <cstddef>
#include <optional>
#include <span>
#include "ilp_for.hpp"

__attribute__((noinline))
std::optional<std::size_t> find_value_no_ctrl_ilp(std::span<const int> data, int target) {
    ILP_FOR_RET_SIMPLE(std::size_t, i, 0uz, data.size(), 4) {
        if (data[i] == target) return i;
        return std::nullopt;
    } ILP_END_RET;
    return std::nullopt;
}
