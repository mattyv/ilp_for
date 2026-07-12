// COMPILE_FAIL: is deprecated
// GCC_ONLY
// EXTRA_FLAGS: -Werror=deprecated-declarations
//
// Pins ilp::detail::warn_gcc_find_block_cliff (find.hpp) - an explicit find_if
// block size N > find_block_gcc_cliff_threshold (16) must trigger GCC's
// deprecation warning about falling off the measured SLP auto-vectorization
// cliff. See docs/PERFORMANCE.md's "Why two shapes" section for the mechanism
// (GCC only vectorizes the blockcheck shape via SLP over one native vector
// group; a 2+-vector-group block is reported "missed: may need non-SLP
// handling" and left effectively unvectorized). GCC-only: the warning is
// gated on __GNUC__ && !defined(__clang__), so it never fires under Clang -
// see check_compile_fail.sh's GCC_ONLY marker.
//
// This also pins find.hpp's include placement in ilp_for.hpp: find.hpp must
// stay included BEFORE the #pragma GCC system_header block, or GCC would
// silently swallow the warning (system headers don't warn) and this case
// would start failing for the wrong reason (COMPILE_OK instead of the
// expected deprecation error).
#include "../../ilp_for.hpp"
#include <vector>

int main() {
    std::vector<int> data(40, 0);
    auto it = ilp::find_if<32>(data, [](int v) { return v != 0; });
    return it == data.end() ? 1 : 0;
}
