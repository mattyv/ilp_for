// Copy count verification tests
// Ensures minimal copies occur in return paths
// Expected copy counts per function:
// - for_loop<T> (with return type): 0 copies, 1 move

#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <numeric>
#include <ranges>
#include <vector>

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
// Non-trivially destructible type tests (must use ILP_FOR_T)
// =============================================================================

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
