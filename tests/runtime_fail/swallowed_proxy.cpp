// RUN_ABORT: swallowed return value
// A value-bearing Proxy must be converted, not discarded as an expression.
#include "../../ilp_for.hpp"

int main() {
    auto result = ilp::for_loop<2>(0, 1, [](int, ilp::ForCtrl& ctrl) {
        ctrl.return_with(42);
    });
    if (result)
        *result;
}
