// RUN_OK
// Negative-space coverage for the unconsumed-Proxy check (DESIGN_NOTES.md
// item 5): the abort is gated on a value actually being PRESENT, so none of
// these flows may trigger it.
//   1. Discarding an EMPTY result's Proxy (`*std::move(r);` when the body never
//      called return_with) is a silent no-op - there is nothing to swallow.
//      Before this gating, it false-positive aborted.
//   2. Copying a live Proxy transfers the consume obligation to the copy: the
//      source counts as consumed by its copy, and converting the copy exactly
//      once satisfies the check. (Mainly for MSVC, whose conversions are
//      lvalue-callable so proxies can be named and copied - but the copy
//      constructor semantics are identical on GCC/Clang, tested here.)
//   3. The normal documented flow (`if (r) return/use *std::move(r);`).
#include "../../ilp_for.hpp"
#include <utility>

int main() {
    // 1. empty result, bare discard - must not abort
    {
        auto r = ilp::for_loop<4>(0, 10, [](auto, auto&) {});
        *std::move(r);
    }
    // 1b. empty result, guarded discard - the guard is false, but the pattern
    //     must stay abort-free if a Proxy is ever materialized on a dead branch
    {
        auto r = ilp::for_loop<4>(0, 10, [](auto, auto&) {});
        if (r) { *std::move(r); }
    }
    // 2. copy transfers the consume obligation
    {
        auto r = ilp::for_loop<4>(0, 10, [](auto i, auto& ctrl) {
            if (i == 3) ctrl.return_with(7);
        });
        auto p = *std::move(r);
        auto q = p; // p now counts as consumed by q
        int v = std::move(q);
        if (v != 7) return 2;
    }
    // 3. normal consume, untyped and typed
    {
        auto r = ilp::for_loop<4>(0, 10, [](auto i, auto& ctrl) {
            if (i == 3) ctrl.return_with(42);
        });
        if (r) {
            int v = *std::move(r);
            if (v != 42) return 3;
        }
        auto tr = ilp::for_loop_typed<int, 4>(0, 10, [](auto i, auto& ctrl) {
            if (i == 2) ctrl.return_with(9);
        });
        if (tr) {
            int v = *std::move(tr);
            if (v != 9) return 4;
        }
    }
    return 0;
}
