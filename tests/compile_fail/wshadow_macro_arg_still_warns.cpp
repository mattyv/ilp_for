// COMPILE_FAIL: shadow
// EXTRA_FLAGS: -Wshadow -Werror=shadow
// The load-bearing half of the system_header suppression contract
// (DESIGN_NOTES.md item 4): user shadowing introduced via a MACRO-ARGUMENT
// token must still warn. The loop-variable declaration below is a macro
// argument - it keeps its user-file spelling location even though it lands
// inside the (system-header) macro expansion as a lambda parameter, so the
// shadow diagnostic must still fire on it. Companion to
// wshadow_user_still_warns.cpp, which covers shadowing in plain non-macro
// code. On Clang this category is -Wshadow-uncaptured-local (part of
// -Wshadow-all, not plain -Wshadow - see the item 4 reproduction table), so
// enable it via an in-file pragma rather than a flag GCC would reject.
#if defined(__clang__)
#pragma clang diagnostic error "-Wshadow-uncaptured-local"
#endif
#include "../../ilp_for.hpp"

int macro_arg_shadow(int n) {
    int i = 0; // WRONG (deliberately): shadowed by the ILP_FOR loop variable below
    ILP_FOR(auto i, 0, n, 4) {
        (void)i;
    }
    ILP_END;
    return i;
}

int main() { return macro_arg_shadow(4); }
