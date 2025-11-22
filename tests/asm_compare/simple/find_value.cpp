#include <cstddef>
#include <span>
#include <optional>

__attribute__((noinline))
std::optional<std::size_t> find_value_simple(std::span<const unsigned> arr, unsigned target) {
    for (std::size_t i = 0; i < arr.size(); ++i) {
        if (arr[i] == target) {
            return i;
        }
    }
    return std::nullopt;
}
