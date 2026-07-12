// COMPILE_FAIL: no matching function
// ilp::find_if requires std::ranges::random_access_range; a std::list only
// offers bidirectional iterators, so this must fail to compile via overload
// resolution / constraint failure rather than some deeper instantiation error.
#include "../../ilp_for.hpp"
#include <list>

int main() {
    std::list<int> data = {1, 2, 3};
    auto it = ilp::find_if<8>(data, [](int v) { return v == 2; });
    return it == data.end() ? 1 : 0;
}
