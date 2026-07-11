// COMPILE_FAIL: change ILP_END to ILP_END_RETURN
// ILP_RETURN used inside a loop closed with plain ILP_END (should be
// ILP_END_RETURN). Must fail to compile with the fix-it message.
#include "../../ilp_for.hpp"

int find_index(const int* data, int n, int target) {
    ILP_FOR(auto i, 0, n, 4) {
        if (data[i] == target) ILP_RETURN(i);
    } ILP_END; // WRONG: should be ILP_END_RETURN
    return -1;
}

int main() {
    int data[] = {1, 2, 3};
    return find_index(data, 3, 2);
}
