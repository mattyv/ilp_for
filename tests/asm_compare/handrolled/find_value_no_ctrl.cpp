#include <cstddef>
#include <optional>
#include <span>

__attribute__((noinline))
std::optional<std::size_t> find_value_no_ctrl_handrolled(std::span<const int> data, int target) {
    std::size_t n = data.size();
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        if (data[i] == target) return i;
        if (data[i + 1] == target) return i + 1;
        if (data[i + 2] == target) return i + 2;
        if (data[i + 3] == target) return i + 3;
    }
    for (; i < n; ++i) {
        if (data[i] == target) return i;
    }
    return std::nullopt;
}
