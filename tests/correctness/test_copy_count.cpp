// Copy count verification tests
// Ensures minimal copies occur in return paths
// Expected copy counts per function:
// - for_loop_ret: 0 copies, 1 move
// - find (speculative): 0 copies, 1 move (with std::move(results[j]))
// - reduce: 1 copy (R result = init), N moves

#include "catch.hpp"
#include "../../ilp_for.hpp"

namespace {

// Tracks copy/move operations at runtime
struct CopyMoveCounter {
    static inline int copies = 0;
    static inline int moves = 0;
    int value = 0;

    CopyMoveCounter() = default;
    explicit CopyMoveCounter(int v) : value(v) {}
    CopyMoveCounter(const CopyMoveCounter& o) : value(o.value) { ++copies; }
    CopyMoveCounter(CopyMoveCounter&& o) noexcept : value(o.value) { ++moves; }
    CopyMoveCounter& operator=(const CopyMoveCounter& o) { value = o.value; ++copies; return *this; }
    CopyMoveCounter& operator=(CopyMoveCounter&& o) noexcept { value = o.value; ++moves; return *this; }

    static void reset() { copies = moves = 0; }

    bool operator==(const CopyMoveCounter& o) const { return value == o.value; }
};

// Move-only type - compilation fails if copy is attempted
struct MoveOnly {
    int value = 0;

    MoveOnly() = default;
    explicit MoveOnly(int v) : value(v) {}
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;

    bool operator==(const MoveOnly& o) const { return value == o.value; }
};

// Binary operation for CopyMoveCounter - takes const refs to avoid copies
CopyMoveCounter add_counters(const CopyMoveCounter& a, const CopyMoveCounter& b) {
    return CopyMoveCounter(a.value + b.value);
}

// Binary operation for MoveOnly
MoveOnly add_move_only(MoveOnly&& a, MoveOnly&& b) {
    return MoveOnly(a.value + b.value);
}

} // namespace

// =============================================================================
// for_loop_ret copy count tests
// =============================================================================

TEST_CASE("No copies in for_loop_ret return path", "[copy_count]") {
    CopyMoveCounter::reset();

    auto result = ilp::for_loop_ret<CopyMoveCounter, 4>(0, 10,
        [](int i, ilp::LoopCtrl<CopyMoveCounter>& ctrl) {
            if (i == 5) {
                ctrl.return_value = CopyMoveCounter(i * 10);
                ctrl.ok = false;
            }
        });

    REQUIRE(result.has_value());
    CHECK(result->value == 50);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Move-only type works with for_loop_ret", "[copy_count][compile-time]") {
    // If this compiles, no copies are attempted
    auto result = ilp::for_loop_ret<MoveOnly, 4>(0, 10,
        [](int i, ilp::LoopCtrl<MoveOnly>& ctrl) {
            if (i == 5) {
                ctrl.return_value = MoveOnly(i * 10);
                ctrl.ok = false;
            }
        });

    REQUIRE(result.has_value());
    CHECK(result->value == 50);
}

// =============================================================================
// find copy count tests
// =============================================================================

TEST_CASE("No copies in find with optional return", "[copy_count]") {
    CopyMoveCounter::reset();

    auto result = ilp::find<4>(0, 10, [](int i, int) -> std::optional<CopyMoveCounter> {
        if (i == 5) {
            return CopyMoveCounter(i * 10);
        }
        return std::nullopt;
    });

    REQUIRE(result.has_value());
    CHECK(result->value == 50);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Move-only type works with find", "[copy_count][compile-time]") {
    auto result = ilp::find<4>(0, 10, [](int i, int) -> std::optional<MoveOnly> {
        if (i == 5) {
            return std::make_optional(MoveOnly(i * 10));
        }
        return std::nullopt;
    });

    REQUIRE(result.has_value());
    CHECK(result->value == 50);
}

// =============================================================================
// reduce copy count tests
// =============================================================================

TEST_CASE("Minimal copies in reduce with ctrl", "[copy_count][reduce]") {
    CopyMoveCounter::reset();

    // Copies from reduce with N=4:
    // - init passed by value to reduce_impl
    // - init passed by value to operation_identity
    // - accs.fill(identity) copies identity to N accumulators
    // - R result = init in final fold
    // Total: ~7 copies for N=4

    auto result = ilp::reduce<4>(0, 4, CopyMoveCounter{0}, add_counters,
        [](int i, ilp::LoopCtrl<void>&) {
            return CopyMoveCounter(i);
        });

    // 0 + 1 + 2 + 3 = 6
    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    // N+3 copies expected: N for accs.fill + init copies in function calls
    CHECK(CopyMoveCounter::copies <= 10);  // Reasonable for reduce with N accumulators
}

TEST_CASE("Minimal copies in reduce without ctrl", "[copy_count][reduce]") {
    CopyMoveCounter::reset();

    auto result = ilp::reduce<4>(0, 4, CopyMoveCounter{0}, add_counters,
        [](int i) {
            return CopyMoveCounter(i);
        });

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies <= 10);  // Reasonable for reduce with N accumulators
}

// =============================================================================
// Range-based copy count tests
// =============================================================================

TEST_CASE("No copies in for_loop_range_ret", "[copy_count][range]") {
    CopyMoveCounter::reset();
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto result = ilp::for_loop_range_ret<CopyMoveCounter, 4>(data,
        [](int val, ilp::LoopCtrl<CopyMoveCounter>& ctrl) {
            if (val == 5) {
                ctrl.return_value = CopyMoveCounter(val * 10);
                ctrl.ok = false;
            }
        });

    REQUIRE(result.has_value());
    CHECK(result->value == 50);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Minimal copies in reduce_range return path", "[copy_count][range][reduce]") {
    CopyMoveCounter::reset();
    std::vector<int> data = {0, 1, 2, 3};

    auto result = ilp::reduce_range<4>(data, CopyMoveCounter{0}, add_counters,
        [](int val, ilp::LoopCtrl<void>&) {
            return CopyMoveCounter(val);
        });

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies <= 10);  // Reasonable for reduce with N accumulators
}

// =============================================================================
// Macro-based copy count tests (via helper functions)
// =============================================================================

namespace {
    std::optional<CopyMoveCounter> test_ilp_for_ret_helper() {
        CopyMoveCounter::reset();
        ILP_FOR_RET(CopyMoveCounter, auto i, 0, 10, 4) {
            if (i == 5) {
                ILP_RETURN(CopyMoveCounter(i * 10));
            }
        } ILP_END_RET;
        return std::nullopt;
    }

    std::optional<MoveOnly> test_ilp_for_ret_move_only_helper() {
        ILP_FOR_RET(MoveOnly, auto i, 0, 10, 4) {
            if (i == 5) {
                ILP_RETURN(MoveOnly(i * 10));
            }
        } ILP_END_RET;
        return std::nullopt;
    }
}

TEST_CASE("No copies in ILP_FOR_RET macro", "[copy_count][macro]") {
    auto result = test_ilp_for_ret_helper();
    REQUIRE(result.has_value());
    CHECK(result->value == 50);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Move-only type works with ILP_FOR_RET macro", "[copy_count][macro][compile-time]") {
    auto result = test_ilp_for_ret_move_only_helper();
    REQUIRE(result.has_value());
    CHECK(result->value == 50);
}
