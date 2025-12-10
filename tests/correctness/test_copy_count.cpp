// Copy count verification tests
// Ensures minimal copies occur in return paths
// Expected copy counts per function:
// - for_loop<T> (with return type): 0 copies, 1 move
// - find (speculative): 0 copies, 1 move (with std::move(results[j]))
// - reduce: 1 copy (R result = init), N moves

#include "../../ilp_for.hpp"
#include "catch.hpp"

#if !defined(ILP_MODE_SIMPLE)

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
        CopyMoveCounter& operator=(const CopyMoveCounter& o) {
            value = o.value;
            ++copies;
            return *this;
        }
        CopyMoveCounter& operator=(CopyMoveCounter&& o) noexcept {
            value = o.value;
            ++moves;
            return *this;
        }

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
        friend MoveOnly operator+(const MoveOnly& a, const MoveOnly& b) { return MoveOnly(a.value + b.value); }
    };

    // Binary operation for CopyMoveCounter - takes const refs to avoid copies
    CopyMoveCounter add_counters(const CopyMoveCounter& a, const CopyMoveCounter& b) {
        return CopyMoveCounter(a.value + b.value);
    }

} // namespace

// =============================================================================
// for_loop with type-erased return path copy count tests
// =============================================================================

TEST_CASE("No copies in for_loop return path", "[copy_count]") {
    CopyMoveCounter::reset();

    auto result = ilp::for_loop<4>(0, 10, [](int i, ilp::ForCtrl& ctrl) {
        if (i == 5) {
            ctrl.storage.set(CopyMoveCounter(i * 10));
            ctrl.return_set = true;
            ctrl.ok = false;
        }
    });

    REQUIRE(result.has_return);
    CopyMoveCounter value = *std::move(result);
    CHECK(value.value == 50);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Move-only type works with for_loop", "[copy_count][compile-time]") {
    // If this compiles, no copies are attempted
    auto result = ilp::for_loop<4>(0, 10, [](int i, ilp::ForCtrl& ctrl) {
        if (i == 5) {
            ctrl.storage.set(MoveOnly(i * 10));
            ctrl.return_set = true;
            ctrl.ok = false;
        }
    });

    REQUIRE(result.has_return);
    MoveOnly value = *std::move(result);
    CHECK(value.value == 50);
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

    auto result = ilp::reduce<4>(0, 4, CopyMoveCounter{0}, add_counters, [](int i) { return CopyMoveCounter(i); });

    // 0 + 1 + 2 + 3 = 6
    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    // ILP mode: N-1 copies for unknown ops (move first, copy rest)
    CHECK(CopyMoveCounter::copies == 3);
}

TEST_CASE("Minimal copies in reduce without ctrl", "[copy_count][reduce]") {
    CopyMoveCounter::reset();

    auto result = ilp::reduce<4>(0, 4, CopyMoveCounter{0}, add_counters, [](int i) { return CopyMoveCounter(i); });

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    // ILP mode: N-1 copies for unknown ops (move first, copy rest)
    CHECK(CopyMoveCounter::copies == 3);
}

// =============================================================================
// Range-based copy count tests
// =============================================================================

TEST_CASE("No copies in for_loop_range", "[copy_count][range]") {
    CopyMoveCounter::reset();
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto result = ilp::for_loop_range<4>(data, [](int val, ilp::ForCtrl& ctrl) {
        if (val == 5) {
            ctrl.storage.set(CopyMoveCounter(val * 10));
            ctrl.return_set = true;
            ctrl.ok = false;
        }
    });

    REQUIRE(result.has_return);
    CopyMoveCounter value = *std::move(result);
    CHECK(value.value == 50);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Minimal copies in reduce_range return path - plain return (transform_reduce)",
          "[copy_count][range][reduce]") {
    CopyMoveCounter::reset();
    std::vector<int> data = {0, 1, 2, 3};

    // Plain return: uses transform_reduce path (SIMD optimized)
    auto result =
        ilp::reduce_range<4>(data, CopyMoveCounter{0}, add_counters, [](int val) { return CopyMoveCounter(val); });

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    // transform_reduce handles accumulator internally - zero copies
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Minimal copies in reduce_range return path - std::optional (nested loops)", "[copy_count][range][reduce]") {
    CopyMoveCounter::reset();
    std::vector<int> data = {0, 1, 2, 3};

    // std::optional return: uses nested loops path (supports early break)
    auto result = ilp::reduce_range<4>(data, CopyMoveCounter{0}, add_counters,
                                       [](int val) -> std::optional<CopyMoveCounter> { return CopyMoveCounter(val); });

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    // Nested loops path requires copies for accumulator management
    CHECK(CopyMoveCounter::copies == 3); // ILP mode: one copy per accumulator slot
}

// =============================================================================
// Macro-based copy count tests (via helper functions)
// =============================================================================

namespace {
    std::optional<CopyMoveCounter> test_ilp_for_helper() {
        CopyMoveCounter::reset();
        ILP_FOR(auto i, 0, 10, 4) {
            if (i == 5) {
                ILP_RETURN(CopyMoveCounter(i * 10));
            }
        }
        ILP_END_RETURN;
        return std::nullopt;
    }

    std::optional<MoveOnly> test_ilp_for_move_only_helper() {
        ILP_FOR(auto i, 0, 10, 4) {
            if (i == 5) {
                ILP_RETURN(MoveOnly(i * 10));
            }
        }
        ILP_END_RETURN;
        return std::nullopt;
    }
} // namespace

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
// Reduce copy count tests
// =============================================================================

TEST_CASE("Plain return reduce has minimal copy overhead", "[copy_count][reduce]") {
    CopyMoveCounter::reset();

    auto result = ilp::reduce_auto<ilp::LoopType::Sum>(0, 4, CopyMoveCounter{0}, add_counters,
                                                       [&](auto i) { return CopyMoveCounter(i); });

    CHECK(result.value == 6); // 0+1+2+3
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 3);
}

TEST_CASE("std::optional reduce has minimal copy overhead", "[copy_count][reduce]") {
    CopyMoveCounter::reset();

    auto result = ilp::reduce_auto<ilp::LoopType::Sum>(0, 10, CopyMoveCounter{0}, add_counters,
                                                       [&](auto i) -> std::optional<CopyMoveCounter> {
                                                           if (i >= 4)
                                                               return std::nullopt;
                                                           return CopyMoveCounter(i);
                                                       });

    CHECK(result.value == 6); // 0+1+2+3
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 3);
}

// Note: Move-only types require known ops (std::plus<>, etc.) which use
// make_identity for zero-copy accumulator initialization. Unknown ops
// (custom lambdas) still require copying the identity value.

TEST_CASE("Range-based reduce copy count with plain return", "[copy_count][reduce][range]") {
    CopyMoveCounter::reset();
    std::vector<int> data = {0, 1, 2, 3};

    // Plain return uses transform_reduce → 0 copies
    auto result = ilp::reduce_range_auto<ilp::LoopType::Sum>(data, CopyMoveCounter{0}, add_counters,
                                                             [&](auto val) { return CopyMoveCounter(val); });

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    // transform_reduce handles accumulator internally - zero copies
    CHECK(CopyMoveCounter::copies == 0);
}

// =============================================================================
// Zero-copy tests for known operations (std::plus, std::multiplies, etc.)
// =============================================================================

TEST_CASE("Zero copies with std::plus<> known identity", "[copy_count][reduce][zero_copy]") {
    CopyMoveCounter::reset();

    // std::plus<> is a known operation with compile-time identity (T{} = 0)
    // Accumulators are directly initialized via make_identity, skipping fill
    auto result = ilp::reduce<4>(0, 4, CopyMoveCounter{0}, std::plus<>{}, [](int i) { return CopyMoveCounter(i); });

    CHECK(result.value == 6); // 0+1+2+3
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    // 0 copies: make_identity returns prvalue, guaranteed copy elision into array elements
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Zero copies with std::plus<> and ctrl", "[copy_count][reduce][zero_copy]") {
    CopyMoveCounter::reset();

    auto result = ilp::reduce<4>(0, 4, CopyMoveCounter{0}, std::plus<>{}, [](int i) { return CopyMoveCounter(i); });

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Zero copies with plain return and std::plus<>", "[copy_count][reduce][zero_copy]") {
    CopyMoveCounter::reset();

    auto result = ilp::reduce_auto<ilp::LoopType::Sum>(0, 4, CopyMoveCounter{0}, std::plus<>{},
                                                       [&](auto i) { return CopyMoveCounter(i); });

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Zero copies with range-based reduce and std::plus<>", "[copy_count][reduce][range][zero_copy]") {
    CopyMoveCounter::reset();
    std::vector<int> data = {0, 1, 2, 3};

    auto result =
        ilp::reduce_range<4>(data, CopyMoveCounter{0}, std::plus<>{}, [](int val) { return CopyMoveCounter(val); });

    CHECK(result.value == 6);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Move-only type works with std::plus<> reduce", "[copy_count][reduce][zero_copy][compile-time]") {
    // This compiles because std::plus<> uses make_identity (zero copies)
    // instead of accs.fill() which would require copy constructor
    auto result = ilp::reduce<4>(0, 4, MoveOnly{0}, std::plus<>{}, [](int i) { return MoveOnly(i); });

    CHECK(result.value == 6);
}

// =============================================================================
// Non-trivially destructible type tests (must use ILP_FOR_T)
// =============================================================================

namespace {
    // Type with non-trivial destructor that tracks destruction
    struct DestructorTracker {
        static inline int destructor_calls = 0;
        int value = 0;

        DestructorTracker() = default;
        explicit DestructorTracker(int v) : value(v) {}
        DestructorTracker(const DestructorTracker& o) : value(o.value) {}
        DestructorTracker(DestructorTracker&& o) noexcept : value(o.value) { o.value = -1; }
        DestructorTracker& operator=(const DestructorTracker&) = default;
        DestructorTracker& operator=(DestructorTracker&&) noexcept = default;
        ~DestructorTracker() { ++destructor_calls; }

        static void reset() { destructor_calls = 0; }
    };

    static_assert(!std::is_trivially_destructible_v<DestructorTracker>,
                  "DestructorTracker must be non-trivially destructible for this test");

    std::optional<DestructorTracker> test_ilp_for_t_nontrivial_helper() {
        ILP_FOR_T(DestructorTracker, auto i, 0, 10, 4) {
            if (i == 5) {
                ILP_RETURN(DestructorTracker(i * 10));
            }
        }
        ILP_END_RETURN;
        return std::nullopt;
    }
} // namespace

TEST_CASE("ILP_FOR_T properly destructs non-trivially destructible return types", "[copy_count][destructor]") {
    DestructorTracker::reset();

    {
        auto result = test_ilp_for_t_nontrivial_helper();
        REQUIRE(result.has_value());
        CHECK(result->value == 50);
    }

    // Destructor should have been called at least once (for the stored object)
    // The exact count depends on move elision, but it must be > 0
    INFO("Destructor calls: " << DestructorTracker::destructor_calls);
    CHECK(DestructorTracker::destructor_calls > 0);
}

TEST_CASE("TypedStorage properly destructs stored object", "[copy_count][destructor]") {
    DestructorTracker::reset();

    {
        ilp::TypedStorage<DestructorTracker> storage;
        storage.set(DestructorTracker(42));
        // At this point, object is constructed in storage

        DestructorTracker extracted = storage.extract();
        // extract() should have called destructor on the stored object
        CHECK(extracted.value == 42);
    }

    // Destructor called: once in extract(), once for 'extracted' going out of scope
    INFO("Destructor calls: " << DestructorTracker::destructor_calls);
    CHECK(DestructorTracker::destructor_calls >= 2);
}

#endif // !ILP_MODE_SIMPLE
