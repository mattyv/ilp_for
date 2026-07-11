// RUN_ABORT: stored a value
// The DESIGN_NOTES.md item 3 repro verbatim: ILP_RETURN(int) in a
// long-returning function. The untyped SBO recovers the value as whatever
// type the enclosing function returns, not the type that was stored - a
// silent pun in release builds. In debug builds (this harness never defines
// NDEBUG) it must abort with a message naming both types.
#include "../../ilp_for.hpp"
#include <vector>

long find_it(const std::vector<int>& data, int target) {
    ILP_FOR(auto i, 0, (int)data.size(), 4) {
        if (data[i] == target) ILP_RETURN(i); // stores int, function returns long
    } ILP_END_RETURN;
    return -1;
}

int main() {
    std::vector<int> d{1, 2, 3, 42};
    return static_cast<int>(find_it(d, 42));
}
