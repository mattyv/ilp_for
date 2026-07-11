// RUN_OK
// EXTRA_FLAGS: -DNDEBUG
// Zero-overhead proof, kept permanently in CI: under -DNDEBUG the type-check
// gate is off, so SmallStorage must have exactly arch::sbo_size bytes - no
// tag pointer, no layout change from the debug-mode instrumentation.
#include "../../ilp_for.hpp"

static_assert(ILP_TYPECHECK_ENABLED == 0, "expected the type check to be disabled under NDEBUG");
static_assert(sizeof(ilp::SmallStorage) == ilp::arch::sbo_size,
              "SmallStorage must have zero overhead in release (-DNDEBUG) builds");

int main() {
    return 0;
}
