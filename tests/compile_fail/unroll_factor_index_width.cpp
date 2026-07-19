// COMPILE_FAIL: Unroll factor N must be representable by the loop index type
#include "../../ilp_for.hpp"
#include <cstdint>

int main() {
    ilp::for_each<256>(std::uint8_t{0}, std::uint8_t{10}, [](std::uint8_t, auto&) {});
}
