#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>

__attribute__((noinline)) std::optional<std::size_t> find_value_std(std::span<const unsigned> arr, unsigned target) {
    auto it = std::find(arr.begin(), arr.end(), target);
    if (it != arr.end()) {
        return static_cast<std::size_t>(it - arr.begin());
    }
    return std::nullopt;
}
