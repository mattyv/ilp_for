// COMPILE_FAIL: use ilp::for_loop
// ilp::for_each is break/continue-only; calling return_with must fail to
// compile with a message pointing at ilp::for_loop instead.
#include "../../ilp_for.hpp"

int main() {
    int found = -1;
    ilp::for_each<4>(0, 10, [&](int i, auto& ctrl) {
        if (i == 5)
            ctrl.return_with(i); // WRONG: for_each cannot return a value
        else
            found = i;
    });
    return found;
}
