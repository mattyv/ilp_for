// COMPILE_FAIL: shadow
// EXTRA_FLAGS: -Wshadow -Werror=shadow
// A user's own shadowing (unrelated to any ILP_FOR expansion) must still be
// caught - the library's #pragma GCC system_header only covers tokens spelled
// in its own macro definitions; the user's own code is an ordinary translation
// unit and stays fully warned under -Wshadow. Must fail to compile.
#include "../../ilp_for.hpp"

int user_shadow(int x) {
    int y = x;
    {
        int y = x + 1; // WRONG (deliberately): shadows the outer 'y'
        return y;
    }
}

int main() {
    return user_shadow(1);
}
