// RUN_OK
#include "../../ilp_for.hpp"
#include <cstdint>
#include <initializer_list>
#include <utility>

template<typename T, ilp::Mode M>
T find_index(T target) {
    auto result = ilp::for_loop<4, M>(T{0}, T{5}, [&](auto index, auto& ctrl) {
        if (index == target)
            return ctrl.return_with(index);
    });
    if (result)
        return *std::move(result);
    return T{0};
}

template<typename T>
bool check() {
    for (T target : {T{0}, T{3}, T{4}}) {
        if (find_index<T, ilp::Mode::Simple>(target) != target ||
            find_index<T, ilp::Mode::Unrolled>(target) != target)
            return false;
    }
    return true;
}

int main() {
    return check<std::int8_t>() && check<std::uint8_t>() && check<std::int16_t>() && check<std::uint16_t>() ? 0 : 1;
}
