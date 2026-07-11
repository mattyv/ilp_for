// COMPILE_FAIL: change ILP_END to ILP_END_RETURN
// Same mismatch as mismatch_end.cpp, but through the typed (ILP_FOR_T) macro.
#include "../../ilp_for.hpp"

struct Result {
    int index;
};

Result find_result(const int* data, int n, int target) {
    ILP_FOR_T(Result, auto i, 0, n, 4) {
        if (data[i] == target) ILP_RETURN(Result{i});
    } ILP_END; // WRONG: should be ILP_END_RETURN
    return Result{-1};
}

int main() {
    int data[] = {1, 2, 3};
    return find_result(data, 3, 2).index;
}
