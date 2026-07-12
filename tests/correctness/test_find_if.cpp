#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <cstdint>
#include <deque>
#include <ranges>
#include <span>
#include <vector>

// Coverage for ilp::find_if - the dedicated vectorizable first-match search
// primitive. Unlike ILP_FOR/ILP_BREAK (which lower to a per-lane body call with
// exit-state tracking that cannot auto-vectorize), find_if's N=0 default
// resolves to one of two shapes, chosen per compiler AND ISA (see
// detail::find_if_resolved_block / docs/PERFORMANCE.md): a two-phase
// blockcheck shape - a branch-free "does this block contain a match?" scan,
// followed by a scalar re-scan of the hit block (or remainder) to locate the
// exact first match - on Clang and on GCC without a ptest-capable ISA; or the
// PLAIN scalar loop (no block phase at all) on GCC 15+ with SSE4.1+/AArch64,
// deferring to GCC's own early-break loop vectorizer. An explicit N always
// forces the blockcheck shape on every compiler. See ilp_for/detail/find.hpp
// for the implementation.
//
// Predicate purity note (tested behaviorally, not by counting invocations):
// the predicate may be invoked on elements past the first match, and more than
// once per element, so tests below must not assert invocation counts - only
// the returned iterator/index.

using ModeUnrolledTag = std::integral_constant<ilp::Mode, ilp::Mode::Unrolled>;
using ModeSimpleTag = std::integral_constant<ilp::Mode, ilp::Mode::Simple>;

namespace {
    // Builds a vector of `size` ints, all zero except `match_indices`, which are
    // set to 1. The predicate under test is `[](int v){ return v != 0; }`.
    //
    // Deliberately std::vector<int>(size) (value-initialized to 0), not the
    // std::vector<int>(size, 0) fill-constructor: under Clang's
    // -fsanitize=unsigned-integer-overflow, libstdc++ 15's fill-constructor path
    // (bits/stl_uninitialized.h) trips a documented false positive unrelated to
    // this library - confirmed by reproducing it with a bare
    // std::vector<int>(24, 0), no ilp_for involvement.
    std::vector<int> make_data(std::size_t size, std::initializer_list<std::size_t> match_indices) {
        std::vector<int> data(size);
        for (auto idx : match_indices)
            data[idx] = 1;
        return data;
    }

    auto is_match = [](int v) { return v != 0; };
} // namespace

TEST_CASE("find_if matches at every offset within a block", "[find_if]") {
    // 24 elements, sole match at 8+off for off in 0..7 (explicit N=8).
    for (std::size_t off = 0; off < 8; ++off) {
        std::size_t match_index = 8 + off;
        auto data = make_data(24, {match_index});
        auto it = ilp::find_if<8>(data, is_match);
        REQUIRE(it != data.end());
        REQUIRE(static_cast<std::size_t>(it - data.begin()) == match_index);
    }
}

TEST_CASE("find_if returns end when there is no match", "[find_if]") {
    SECTION("size % N == 0") {
        auto data = make_data(16, {});
        auto it = ilp::find_if<8>(data, is_match);
        REQUIRE(it == data.end());
    }

    SECTION("size % N != 0") {
        auto data = make_data(21, {});
        auto it = ilp::find_if<8>(data, is_match);
        REQUIRE(it == data.end());
    }
}

TEST_CASE("find_if on an empty range returns end", "[find_if]") {
    std::vector<int> data;
    auto it = ilp::find_if<8>(data, is_match);
    REQUIRE(it == data.end());
}

TEST_CASE("find_if matches at the first element", "[find_if]") {
    auto data = make_data(24, {0});
    auto it = ilp::find_if<8>(data, is_match);
    REQUIRE(it == data.begin());
}

TEST_CASE("find_if matches at the last element with size % N != 0", "[find_if]") {
    // N=8, size=21, match at 20 (inside the remainder tail: block_end=16).
    auto data = make_data(21, {20});
    auto it = ilp::find_if<8>(data, is_match);
    REQUIRE(it != data.end());
    REQUIRE(static_cast<std::size_t>(it - data.begin()) == 20);
}

TEST_CASE("find_if matches inside the remainder tail (not just the last element)", "[find_if]") {
    // N=8, size=21: block_end=16, remainder is indices 16..20. Match at 17
    // (block_end + 1).
    auto data = make_data(21, {17});
    auto it = ilp::find_if<8>(data, is_match);
    REQUIRE(it != data.end());
    REQUIRE(static_cast<std::size_t>(it - data.begin()) == 17);
}

TEST_CASE("find_if returns the FIRST match when a block contains several", "[find_if]") {
    // N=8, matches at 9, 10, 14, and 20 -> assert 9 (pins re-scan semantics: the
    // block-level "any match?" check only tells us the block was hit, the
    // scalar re-scan must still find the earliest one).
    auto data = make_data(24, {9, 10, 14, 20});
    auto it = ilp::find_if<8>(data, is_match);
    REQUIRE(it != data.end());
    REQUIRE(static_cast<std::size_t>(it - data.begin()) == 9);
}

TEST_CASE("find_if handles a range smaller than the block size", "[find_if]") {
    // N=16, size=5, match at 3: entirely handled by the remainder/scalar loop.
    auto data = make_data(5, {3});
    auto it = ilp::find_if<16>(data, is_match);
    REQUIRE(it != data.end());
    REQUIRE(static_cast<std::size_t>(it - data.begin()) == 3);
}

TEMPLATE_TEST_CASE("find_if Mode::Unrolled and Mode::Simple agree", "[find_if][mode]", ModeUnrolledTag,
                    ModeSimpleTag) {
    constexpr ilp::Mode M = TestType::value;

    SECTION("matches at every offset within a block") {
        for (std::size_t off = 0; off < 8; ++off) {
            std::size_t match_index = 8 + off;
            auto data = make_data(24, {match_index});
            auto it = ilp::find_if<8, M>(data, is_match);
            REQUIRE(it != data.end());
            REQUIRE(static_cast<std::size_t>(it - data.begin()) == match_index);
        }
    }

    SECTION("no match returns end") {
        auto data = make_data(16, {});
        auto it = ilp::find_if<8, M>(data, is_match);
        REQUIRE(it == data.end());
    }

    SECTION("first match wins when a block contains several") {
        auto data = make_data(24, {9, 10, 14, 20});
        auto it = ilp::find_if<8, M>(data, is_match);
        REQUIRE(it != data.end());
        REQUIRE(static_cast<std::size_t>(it - data.begin()) == 9);
    }
}

TEST_CASE("find_if default block size compiles and finds", "[find_if][default]") {
    auto data = make_data(1000, {500});
    auto it = ilp::find_if(data, is_match); // no explicit N
    REQUIRE(it != data.end());
    REQUIRE(static_cast<std::size_t>(it - data.begin()) == 500);
}

// Pins the per-compiler/per-ISA default-block-size STRATEGY resolved by
// detail::find_if_resolved_block (find.hpp) - see docs/PERFORMANCE.md for the
// measured tables behind each branch. Guarded so the file compiles (and
// asserts the right thing) on every compiler/ISA combination it's built with,
// not just the one used to write these numbers.
TEST_CASE("find_if default block strategy matches the measured per-compiler/ISA resolution",
          "[find_if][default][strategy]") {
#if defined(__clang__) // must precede __GNUC__: clang defines both
    // Clang: blockcheck, block size scaled to the element's byte width,
    // clamped to [32, 128] elements.
    STATIC_REQUIRE(ilp::detail::find_if_resolved_block<std::uint8_t>() == 128);
    STATIC_REQUIRE(ilp::detail::find_if_resolved_block<std::uint16_t>() == 128);
    STATIC_REQUIRE(ilp::detail::find_if_resolved_block<std::uint32_t>() == 64);
    STATIC_REQUIRE(ilp::detail::find_if_resolved_block<std::uint64_t>() == 32);
#elif defined(__GNUC__) && __GNUC__ >= 15 && (defined(__SSE4_1__) || defined(__aarch64__))
    // GCC 15+ on a ptest-capable ISA (SSE4.1+ on x86; AArch64 on Arm - 32-bit
    // ARM/NEON deliberately excluded from this gate, unverified): defers to
    // GCC's own early-break loop vectorizer - resolved block size 0 means
    // "plain loop, no block phase", for every element size.
    STATIC_REQUIRE(ilp::detail::find_if_resolved_block<std::uint8_t>() == 0);
    STATIC_REQUIRE(ilp::detail::find_if_resolved_block<std::uint16_t>() == 0);
    STATIC_REQUIRE(ilp::detail::find_if_resolved_block<std::uint32_t>() == 0);
    STATIC_REQUIRE(ilp::detail::find_if_resolved_block<std::uint64_t>() == 0);
#else
    // Older GCC / GCC on a narrower ISA / MSVC / unknown: conservative
    // SLP-safe blockcheck sizing - min(16 elements, 64 bytes).
    STATIC_REQUIRE(ilp::detail::find_if_resolved_block<std::uint8_t>() == 16);
    STATIC_REQUIRE(ilp::detail::find_if_resolved_block<std::uint16_t>() == 16);
    STATIC_REQUIRE(ilp::detail::find_if_resolved_block<std::uint32_t>() == 16);
    STATIC_REQUIRE(ilp::detail::find_if_resolved_block<std::uint64_t>() == 8);
#endif
}

TEMPLATE_TEST_CASE("find_if default N is correct for uint8_t and uint64_t elements", "[find_if][default]",
                    std::uint8_t, std::uint64_t) {
    // Sized so the SHIPPED block phase actually runs for the resolved default
    // on every strategy branch, not just gets skipped by a too-small range:
    // Clang's u8 default is B=128 (clamp(256/1, 32, 128)) - 40 elements would
    // make block_end=0 and never enter the block loop at all, silently
    // testing only the scalar remainder path. 400 elements with the match at
    // 200 clears every resolved B in this file (up to 128) with room for a
    // full hit-block-then-rescan sequence, and comfortably exceeds the
    // GCC-fallback strategy's u64 default (B=8, since 64/sizeof(u64)==8).
    using T = TestType;
    std::vector<T> data(400, T{0});
    data[200] = T{1};
    auto is_nonzero = [](T v) { return v != T{0}; };

    SECTION("Mode::Unrolled") {
        auto it = ilp::find_if<0, ilp::Mode::Unrolled>(data, is_nonzero);
        REQUIRE(it != data.end());
        REQUIRE(static_cast<std::size_t>(it - data.begin()) == 200);
    }

    SECTION("Mode::Simple") {
        auto it = ilp::find_if<0, ilp::Mode::Simple>(data, is_nonzero);
        REQUIRE(it != data.end());
        REQUIRE(static_cast<std::size_t>(it - data.begin()) == 200);
    }

    SECTION("no match returns end") {
        std::vector<T> no_match(400, T{0});
        auto it = ilp::find_if(no_match, is_nonzero);
        REQUIRE(it == no_match.end());
    }
}

TEST_CASE("find_if works over a non-contiguous random-access range", "[find_if]") {
    // std::deque<int> is random-access but not contiguous: correctness must
    // hold even though this shape won't auto-vectorize.
    std::deque<int> data(24, 0);
    data[13] = 1;
    auto it = ilp::find_if<8>(data, is_match);
    REQUIRE(it != data.end());
    REQUIRE(static_cast<std::size_t>(it - data.begin()) == 13);
}

TEST_CASE("find_if accepts a const range and a const-ref predicate", "[find_if]") {
    const auto data = make_data(24, {11});
    const auto& pred = is_match;
    auto it = ilp::find_if<8>(data, pred);
    REQUIRE(it != data.end());
    REQUIRE(static_cast<std::size_t>(it - data.begin()) == 11);
}

TEST_CASE("find_if on an rvalue non-borrowed range yields std::ranges::dangling", "[find_if]") {
    STATIC_REQUIRE(std::is_same_v<decltype(ilp::find_if<8>(make_data(24, {11}), is_match)), std::ranges::dangling>);
}

TEST_CASE("find_if on an rvalue borrowed range (std::span) returns a usable iterator", "[find_if]") {
    auto data = make_data(24, {11});
    auto it = ilp::find_if<8>(std::span<int>(data), is_match);
    REQUIRE(it != std::span<int>(data).end());
    REQUIRE(static_cast<std::size_t>(it - std::span<int>(data).begin()) == 11);
}

TEST_CASE("find_if works with views::iota for index-style search", "[find_if]") {
    auto data = make_data(24, {17});
    auto indices = std::views::iota(std::size_t{0}, data.size());
    auto it = ilp::find_if<8>(indices, [&](std::size_t i) { return data[i] != 0; });
    REQUIRE(it != indices.end());
    REQUIRE(*it == 17);
}

// This snippet is exactly the one that appears in the README's ilp::find_if
// section and docs/EXAMPLES.md - keep them in sync if either changes.
TEST_CASE("find_if README example: first index where v == 42", "[find_if][readme]") {
    std::vector<int> data = {5, 3, 8, 42, 1, 9};
    auto it = ilp::find_if(data, [](int v) { return v == 42; });
    REQUIRE(it != data.end());
    REQUIRE(static_cast<std::size_t>(it - data.begin()) == 3);
}

TEST_CASE("find_if with block size N=1", "[find_if]") {
    // N=1: every "block" is a single element, degenerating the blockcheck
    // scan to (almost) the scalar loop, but must still be correct.
    auto data = make_data(10, {6});
    auto it = ilp::find_if<1>(data, is_match);
    REQUIRE(it != data.end());
    REQUIRE(static_cast<std::size_t>(it - data.begin()) == 6);
}

TEST_CASE("find_if matches at the last element when size % N == 0 (final block, no remainder)",
          "[find_if]") {
    // N=8, size=16, match at 15: the hit block IS the final block (block_end
    // == size), so there is no remainder loop to fall through to - the
    // scalar re-scan after the block loop must still find it.
    auto data = make_data(16, {15});
    auto it = ilp::find_if<8>(data, is_match);
    REQUIRE(it != data.end());
    REQUIRE(static_cast<std::size_t>(it - data.begin()) == 15);
}

namespace {
    // Truthy, non-bool predicate result: pins the static_cast<bool>(pred(...))
    // in the block-check scan (find.hpp) - a predicate returning a type that's
    // merely convertible to bool (not bool itself) must still work.
    //
    // Deliberately an IMPLICIT operator bool, not explicit: std::indirect_
    // unary_predicate (the constraint on ilp::find_if's Pred) requires
    // std::predicate, whose boolean-testable exposition-only concept requires
    // std::convertible_to<R, bool> - i.e. implicit convertibility. A predicate
    // returning a type with only an EXPLICIT operator bool (like
    // std::optional) fails that constraint before find_if_impl's
    // static_cast<bool> is ever reached, so it can't be exercised here.
    struct Truthy {
        int value;
        operator bool() const { return value != 0; }
    };
} // namespace

TEST_CASE("find_if accepts a predicate returning a non-bool truthy type", "[find_if]") {
    auto data = make_data(24, {11});
    auto truthy_pred = [](int v) { return Truthy{v}; };
    auto it = ilp::find_if<8>(data, truthy_pred);
    REQUIRE(it != data.end());
    REQUIRE(static_cast<std::size_t>(it - data.begin()) == 11);
}
