// COMPILE_OK
// Control case: correct ILP_END_RETURN pairing plus a correct ilp::for_each
// call. Proves the harness isn't passing vacuously - if this ever fails to
// compile, the other cases' failures aren't meaningful either.
#include "../../ilp_for.hpp"
#include <vector>

int find_index(const std::vector<int>& data, int target) {
    ILP_FOR(auto i, std::size_t{0}, data.size(), 4) {
        if (data[i] == target) ILP_RETURN(static_cast<int>(i));
    } ILP_END_RETURN;
    return -1;
}

int sum_until_negative(const std::vector<int>& data) {
    int sum = 0;
    ilp::for_each<4>(std::size_t{0}, data.size(), [&](std::size_t i, auto& ctrl) {
        if (data[i] < 0)
            return ctrl.break_loop();
        sum += data[i];
    });
    return sum;
}

int main() {
    std::vector<int> data = {1, 2, 3, 42, 5};
    if (find_index(data, 42) != 3)
        return 1;
    if (sum_until_negative(data) != 53)
        return 2;
    return 0;
}
