// Copy count verification tests
// Ensures minimal copies occur in return paths
// Expected copy counts per function:
// - for_loop<T> (with return type): 0 copies, 1 move
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

    // Support std::plus<> for zero-copy reduce tests
    friend CopyMoveCounter operator+(const CopyMoveCounter& a, const CopyMoveCounter& b) {
        return CopyMoveCounter(a.value + b.value);
    }
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

    // Support std::plus<> for zero-copy reduce tests
    friend MoveOnly operator+(const MoveOnly& a, const MoveOnly& b) {
        return MoveOnly(a.value + b.value);
    }
};

// Binary operation for CopyMoveCounter - takes const refs to avoid copies
CopyMoveCounter add_counters(const CopyMoveCounter& a, const CopyMoveCounter& b) {
    return CopyMoveCounter(a.value + b.value);
}

} // namespace

// =============================================================================
// for_loop with return type copy count tests
// =============================================================================

TEST_CASE("No copies in for_loop return path", "[copy_count]") {
    CopyMoveCounter::reset();

    auto result = ilp::for_loop<CopyMoveCounter, 4>(0, 10,
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

TEST_CASE("Move-only type works with for_loop", "[copy_count][compile-time]") {
    // If this compiles, no copies are attempted
    auto result = ilp::for_loop<MoveOnly, 4>(0, 10,
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
#if defined(ILP_MODE_SIMPLE) || defined(ILP_MODE_PRAGMA)
    // Single accumulator - no fill needed
    CHECK(CopyMoveCounter::copies == 0);
#else
    // ILP mode: N copies from accs.fill(identity) for unknown ops
    CHECK(CopyMoveCounter::copies == 4);
#endif
}

TEST_CASE("Minimal copies in reduce without ctrl", "[copy_count][reduce]") {
    CopyMoveCounter::reset();

    auto result = ilp::reduce<4>(0, 4, CopyMoveCounter{0}, add_counters,
        [](int i) {
            return CopyMoveCounter(i);
        });

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
#if defined(ILP_MODE_SIMPLE) || defined(ILP_MODE_PRAGMA)
    CHECK(CopyMoveCounter::copies == 0);
#else
    CHECK(CopyMoveCounter::copies == 4);
#endif
}

// =============================================================================
// Range-based copy count tests
// =============================================================================

TEST_CASE("No copies in for_loop_range", "[copy_count][range]") {
    CopyMoveCounter::reset();
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto result = ilp::for_loop_range<CopyMoveCounter, 4>(data,
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

TEST_CASE("Minimal copies in reduce_range return path - plain return (transform_reduce)", "[copy_count][range][reduce]") {
    CopyMoveCounter::reset();
    std::vector<int> data = {0, 1, 2, 3};

    // Plain return: uses transform_reduce path (SIMD optimized)
    auto result = ilp::reduce_range<4>(data, CopyMoveCounter{0}, add_counters,
        [](int val, ilp::LoopCtrl<void>&) {
            return CopyMoveCounter(val);
        });

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    // transform_reduce path is more efficient - no copies needed
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Minimal copies in reduce_range return path - ReduceResult (nested loops)", "[copy_count][range][reduce]") {
    CopyMoveCounter::reset();
    std::vector<int> data = {0, 1, 2, 3};

    // ReduceResult return: uses nested loops path (supports early break)
    auto result = ilp::reduce_range<4>(data, CopyMoveCounter{0}, add_counters,
        [](int val, ilp::LoopCtrl<void>&) {
            return ilp::detail::ReduceResult<CopyMoveCounter>{CopyMoveCounter(val), false};
        });

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    // Nested loops path requires copies for accumulator management
#if defined(ILP_MODE_SIMPLE) || defined(ILP_MODE_PRAGMA)
    CHECK(CopyMoveCounter::copies == 0);  // Simple/pragma modes don't use multiple accumulators
#else
    CHECK(CopyMoveCounter::copies == 4);  // ILP mode: one copy per accumulator slot
#endif
}

// =============================================================================
// Macro-based copy count tests (via helper functions)
// =============================================================================

namespace {
    std::optional<CopyMoveCounter> test_ilp_for_helper() {
        CopyMoveCounter::reset();
        return ILP_FOR(CopyMoveCounter, auto i, 0, 10, 4) {
            if (i == 5) {
                ILP_RETURN(CopyMoveCounter(i * 10));
            }
        } ILP_END;
    }

    std::optional<MoveOnly> test_ilp_for_move_only_helper() {
        return ILP_FOR(MoveOnly, auto i, 0, 10, 4) {
            if (i == 5) {
                ILP_RETURN(MoveOnly(i * 10));
            }
        } ILP_END;
    }
}

TEST_CASE("No copies in ILP_FOR macro with return type", "[copy_count][macro]") {
    auto result = test_ilp_for_helper();
    REQUIRE(result.has_value());
    CHECK(result->value == 50);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Move-only type works with ILP_FOR macro", "[copy_count][macro][compile-time]") {
    auto result = test_ilp_for_move_only_helper();
    REQUIRE(result.has_value());
    CHECK(result->value == 50);
}

// =============================================================================
// ReduceResult wrapper copy count tests
// =============================================================================

TEST_CASE("ReduceResult<Value> has minimal copy overhead", "[copy_count][reduce]") {
    CopyMoveCounter::reset();

    auto result = ILP_REDUCE_AUTO(add_counters, CopyMoveCounter{0}, auto i, 0, 4) {
        ILP_REDUCE_RETURN(CopyMoveCounter(i));
    } ILP_END_REDUCE;

    CHECK(result.value == 6);  // 0+1+2+3
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
#if defined(ILP_MODE_SIMPLE) || defined(ILP_MODE_PRAGMA)
    CHECK(CopyMoveCounter::copies == 0);
#else
    CHECK(CopyMoveCounter::copies == 4);
#endif
}

TEST_CASE("ReduceResult<Break> has minimal copy overhead", "[copy_count][reduce]") {
    CopyMoveCounter::reset();

    auto result = ILP_REDUCE_AUTO(add_counters, CopyMoveCounter{0}, auto i, 0, 10) {
        if (i >= 4) ILP_REDUCE_BREAK;
        ILP_REDUCE_BREAK_VALUE(CopyMoveCounter(i));
    } ILP_END_REDUCE;

    CHECK(result.value == 6);  // 0+1+2+3
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
#if defined(ILP_MODE_SIMPLE) || defined(ILP_MODE_PRAGMA)
    CHECK(CopyMoveCounter::copies == 0);
#else
    CHECK(CopyMoveCounter::copies == 4);
#endif
}

// Note: Move-only types require known ops (std::plus<>, etc.) which use
// make_identity for zero-copy accumulator initialization. Unknown ops
// (custom lambdas) still require copying the identity value.

TEST_CASE("Range-based reduce copy count with ILP_REDUCE_RETURN", "[copy_count][reduce][range]") {
    CopyMoveCounter::reset();
    std::vector<int> data = {0, 1, 2, 3};

    // ILP_REDUCE_RETURN now returns plain value → uses transform_reduce → 0 copies
    auto result = ILP_REDUCE_RANGE_AUTO(add_counters, CopyMoveCounter{0}, auto val, data) {
        ILP_REDUCE_RETURN(CopyMoveCounter(val));
    } ILP_END_REDUCE;

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    // All modes now use transform_reduce for plain returns (0 copies)
    CHECK(CopyMoveCounter::copies == 0);
}

// =============================================================================
// Zero-copy tests for known operations (std::plus, std::multiplies, etc.)
// =============================================================================

TEST_CASE("Zero copies with std::plus<> known identity", "[copy_count][reduce][zero_copy]") {
    CopyMoveCounter::reset();

    // std::plus<> is a known operation with compile-time identity (T{} = 0)
    // Accumulators are directly initialized via make_identity, skipping fill
    auto result = ilp::reduce<4>(0, 4, CopyMoveCounter{0}, std::plus<>{},
        [](int i) {
            return CopyMoveCounter(i);
        });

    CHECK(result.value == 6);  // 0+1+2+3
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    // 0 copies: make_identity returns prvalue, guaranteed copy elision into array elements
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Zero copies with std::plus<> and ctrl", "[copy_count][reduce][zero_copy]") {
    CopyMoveCounter::reset();

    auto result = ilp::reduce<4>(0, 4, CopyMoveCounter{0}, std::plus<>{},
        [](int i, ilp::LoopCtrl<void>&) {
            return CopyMoveCounter(i);
        });

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Zero copies with ILP_REDUCE_RETURN and std::plus<>", "[copy_count][reduce][zero_copy]") {
    CopyMoveCounter::reset();

    auto result = ILP_REDUCE_AUTO(std::plus<>{}, CopyMoveCounter{0}, auto i, 0, 4) {
        ILP_REDUCE_RETURN(CopyMoveCounter(i));
    } ILP_END_REDUCE;

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Zero copies with range-based reduce and std::plus<>", "[copy_count][reduce][range][zero_copy]") {
    CopyMoveCounter::reset();
    std::vector<int> data = {0, 1, 2, 3};

    auto result = ilp::reduce_range<4>(data, CopyMoveCounter{0}, std::plus<>{},
        [](int val, ilp::LoopCtrl<void>&) {
            return CopyMoveCounter(val);
        });

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Move-only type works with std::plus<> reduce", "[copy_count][reduce][zero_copy][compile-time]") {
    // This compiles because std::plus<> uses make_identity (zero copies)
    // instead of accs.fill() which would require copy constructor
    auto result = ilp::reduce<4>(0, 4, MoveOnly{0}, std::plus<>{},
        [](int i) {
            return MoveOnly(i);
        });

    CHECK(result.value == 6);
}
