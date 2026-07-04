// RUN_ABORT: stored a value
// Same-size pun: ILP_RETURN(int) recovered as float. sizeof/alignof match, so
// a size-based check would miss this - the type check is identity-based
// specifically to catch cases like this one.
#include "../../ilp_for.hpp"
#include <vector>

float g(const std::vector<int>& v, int t) {
    ILP_FOR(auto i, 0, (int)v.size(), 4) {
        if (v[i] == t) ILP_RETURN(3); // int stored, float recovered
    } ILP_END_RETURN;
    return -1.f;
}

int main() {
    std::vector<int> v{1, 2, 3};
    return static_cast<int>(g(v, 2));
}
