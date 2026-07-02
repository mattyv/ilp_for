// COMPILE_FAIL: Change the enclosing loop's ILP_END to ILP_END_RETURN
// Inner loop uses ILP_RETURN and is correctly closed with ILP_END_RETURN, but
// the outer loop that contains it is closed with plain ILP_END - a break-only
// loop cannot carry the inner value out. Must fail to compile with the fix-it
// message naming the outer loop's terminator.
#include "../../ilp_for.hpp"

int find_first_match(const int (*rows)[3], int num_rows, int target) {
    ILP_FOR(auto r, 0, num_rows, 2) {
        ILP_FOR(auto c, 0, 3, 4) {
            if (rows[r][c] == target)
                ILP_RETURN(r * 100 + c);
        }
        ILP_END_RETURN;
    }
    ILP_END; // WRONG: outer loop carries an inner ILP_RETURN, must be ILP_END_RETURN
    return -1;
}

int main() {
    int rows[2][3] = {{1, 2, 3}, {4, 5, 6}};
    return find_first_match(rows, 2, 5);
}
