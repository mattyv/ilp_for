#if !defined(ILP_MODE_SIMPLE)
#if !defined(ILP_MODE_SIMPLE)
#include "ilp_for.hpp"
#include <cstddef>
#include <optional>
#include <span>

__attribute__((noinline)) std::optional<std::size_t> find_value_ilp(std::span<const int> data,
                                                                    [[maybe_unused]] int target) {
    ILP_FOR(auto i, 0uz, data.size(), 4) {
        if (data[i] == target)
            ILP_RETURN(i);
    }
    ILP_END_RETURN;
    return std::nullopt;
}
#endif

#endif // !ILP_MODE_SIMPLE
