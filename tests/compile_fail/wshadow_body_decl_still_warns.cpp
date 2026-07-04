// COMPILE_FAIL: shadow
// EXTRA_FLAGS: -Wshadow -Werror=shadow
// The other load-bearing half of the system_header suppression contract
// (DESIGN_NOTES.md item 4): user shadowing declared inside the ILP_FOR BODY
// must still warn. Body tokens are macro arguments spelled at the user site,
// so a declaration there that shadows an enclosing-scope variable keeps its
// user-file location and the shadow diagnostic must still fire. Companion to
// wshadow_macro_arg_still_warns.cpp (loop-variable case) and
// wshadow_nested_ok.cpp (no user shadowing must compile clean). On Clang the
// shadowed outer variable isn't captured by the body lambda, making this
// -Wshadow-uncaptured-local (part of -Wshadow-all, not plain -Wshadow), so
// enable it via an in-file pragma rather than a flag GCC would reject.
#if defined(__clang__)
#pragma clang diagnostic error "-Wshadow-uncaptured-local"
#endif
#include "../../ilp_for.hpp"

int body_decl_shadow(int n) {
    int x = 1;
    ILP_FOR(auto i, 0, n, 4) {
        int x = static_cast<int>(i); // WRONG (deliberately): shadows outer 'x'
        (void)x;
    }
    ILP_END;
    return x;
}

int main() { return body_decl_shadow(4); }
