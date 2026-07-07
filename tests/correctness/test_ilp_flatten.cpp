#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

// Smoke test for ILP_FLATTEN. It guards two things: (1) the macro expands
// cleanly on every CI compiler (empty on non-GCC/Clang, [[gnu::flatten]]
// otherwise) in the real annotation position - before a function that contains
// an ILP_FOR loop with ILP_CONTINUE/ILP_RETURN; and (2) the annotation is
// semantically transparent - it changes inlining, never the observable result.
// The performance effect it exists for (restoring GCC's predicate fusion; see
// docs/PRAGMA_UNROLL.md) is intentionally NOT asserted here - that needs a
// benchmark and a specific compiler/flags, and belongs in benchmarks/.

namespace {
    // Mirrors benchmarks/bench_reduce.cpp's continue_ilp shape: two independent
    // predicates (parity, then threshold), which is exactly the pattern the
    // GCC caveat is about.
    ILP_FLATTEN std::size_t first_odd_over(const std::uint32_t* data, std::size_t size,
                                           std::uint32_t threshold) {
        ILP_FOR(auto i, std::size_t{0}, size, 4) {
            if (data[i] % 2 == 0)
                ILP_CONTINUE;
            if (data[i] > threshold)
                ILP_RETURN(i);
        }
        ILP_END_RETURN;
        return size;
    }
} // namespace

TEST_CASE("ILP_FLATTEN is semantically transparent") {
    std::vector<std::uint32_t> data(1000, 2); // all even -> nothing matches

    SECTION("no match returns size") {
        CHECK(first_odd_over(data.data(), data.size(), 500) == data.size());
    }

    SECTION("finds the first odd value over threshold") {
        data[400] = 501; // odd and > 500
        CHECK(first_odd_over(data.data(), data.size(), 500) == 400);
    }

    SECTION("skips values that fail either predicate") {
        data[100] = 500; // even -> skipped by parity check
        data[200] = 499; // odd but not > 500 -> skipped by threshold check
        data[400] = 501; // the real match
        CHECK(first_odd_over(data.data(), data.size(), 500) == 400);
    }

    SECTION("match in the unrolled remainder (size not a multiple of 4)") {
        std::vector<std::uint32_t> odd_sized(1001, 2);
        odd_sized[1000] = 777; // last element, odd and > threshold
        CHECK(first_odd_over(odd_sized.data(), odd_sized.size(), 500) == 1000);
    }
}
