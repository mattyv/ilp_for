// Copy count verification tests
// Ensures minimal copies occur in return paths
// Expected copy counts per function:
// - for_loop<T> (with return type): 0 copies, 1 move

#include "../../ilp_for.hpp"
#include "catch.hpp"
#include <cstdint>
#include <numeric>
#include <ranges>
#include <vector>


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

    struct SelfAwareSmall {
        static inline bool bad_move = false;
        SelfAwareSmall* self = this;

        SelfAwareSmall() = default;
        SelfAwareSmall(const SelfAwareSmall&) = delete;
        SelfAwareSmall(SelfAwareSmall&& other) noexcept : self(this) {
            bad_move |= other.self != &other;
        }
    };

    struct SelfAwareLarge {
        static inline bool bad_move = false;
        SelfAwareLarge* self = this;
        int padding[8]{};

        SelfAwareLarge() = default;
        SelfAwareLarge(const SelfAwareLarge&) = delete;
        SelfAwareLarge(SelfAwareLarge&& other) noexcept : self(this) {
            bad_move |= other.self != &other;
        }
    };

    struct LiveObject {
        static inline int count = 0;

        LiveObject() { ++count; }
        LiveObject(const LiveObject&) { ++count; }
        LiveObject(LiveObject&&) noexcept { ++count; }
        ~LiveObject() { --count; }
    };

    static_assert(!std::is_trivially_destructible_v<DestructorTracker>,
                  "DestructorTracker must be non-trivially destructible for this test");
    static_assert(!std::is_trivially_copyable_v<SelfAwareSmall>,
                  "SelfAwareSmall must exercise the typed transport path");

} // namespace

// =============================================================================
// Typed return path copy count tests
// =============================================================================

TEST_CASE("No copies in for_loop_typed return path", "[copy_count]") {
    CopyMoveCounter::reset();

    auto result = ilp::for_loop_typed<CopyMoveCounter>(0, 10, [](int i, auto& ctrl) {
        if (i == 5) {
            ctrl.return_with(CopyMoveCounter(i * 10));
        }
    });

    REQUIRE(result.has_return);
    CopyMoveCounter value = *std::move(result);
    CHECK(value.value == 50);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Move-only type works with for_loop_typed", "[copy_count][compile-time]") {
    // If this compiles, no copies are attempted
    auto result = ilp::for_loop_typed<MoveOnly>(0, 10, [](int i, auto& ctrl) {
        if (i == 5) {
            ctrl.return_with(MoveOnly(i * 10));
        }
    });

    REQUIRE(result.has_return);
    MoveOnly value = *std::move(result);
    CHECK(value.value == 50);
}

TEST_CASE("Typed return transport move-constructs small objects", "[copy_count][lifetime]") {
    SelfAwareSmall::bad_move = false;

    auto result = ilp::for_loop_typed<SelfAwareSmall>(0, 1, [](int, auto& ctrl) {
        ctrl.return_with(SelfAwareSmall{});
    });

    [[maybe_unused]] SelfAwareSmall value = *std::move(result);
    CHECK_FALSE(SelfAwareSmall::bad_move);
}

TEST_CASE("Typed return transport move-constructs large objects", "[copy_count][lifetime]") {
    SelfAwareLarge::bad_move = false;

    auto result = ilp::for_loop_typed<SelfAwareLarge>(0, 1, [](int, auto& ctrl) {
        ctrl.return_with(SelfAwareLarge{});
    });

    [[maybe_unused]] SelfAwareLarge value = *std::move(result);
    CHECK_FALSE(SelfAwareLarge::bad_move);
}

TEST_CASE("Discarded typed results destroy their stored object", "[copy_count][lifetime]") {
    LiveObject::count = 0;
    {
        [[maybe_unused]] auto result = ilp::for_loop_typed<LiveObject>(0, 1, [](int, auto& ctrl) {
            ctrl.return_with(LiveObject{});
        });
    }
    CHECK(LiveObject::count == 0);
}

// =============================================================================
// Range-based copy count tests
// =============================================================================

TEST_CASE("No copies in for_loop_range_typed", "[copy_count][range]") {
    CopyMoveCounter::reset();
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto result = ilp::for_loop_range_typed<CopyMoveCounter, 4>(data, [](int val, auto& ctrl) {
        if (val == 5) {
            ctrl.return_with(CopyMoveCounter(val * 10));
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
        ILP_FOR_T(CopyMoveCounter, auto i, 0, 10, 4) {
            if (i == 5) {
                ILP_RETURN(CopyMoveCounter(i * 10));
            }
        }
        ILP_END_RETURN;
        return std::nullopt;
    }

    std::optional<MoveOnly> test_ilp_for_move_only_helper() {
        ILP_FOR_T(MoveOnly, auto i, 0, 10, 4) {
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

    std::optional<SelfAwareSmall> test_ilp_for_nested_self_aware_helper() {
        ILP_FOR_T(SelfAwareSmall, auto i, 0, 2, 4) {
            if (i == 1) {
                ILP_FOR_T(SelfAwareSmall, auto j, 0, 2, 4) {
                    if (j == 1) {
                        ILP_RETURN(SelfAwareSmall{});
                    }
                }
                ILP_END_RETURN;
            }
        }
        ILP_END_RETURN;
        return std::nullopt;
    }

    std::optional<SelfAwareSmall> test_ilp_for_self_aware_helper() {
        ILP_FOR_T(SelfAwareSmall, auto i, 0, 2, 4) {
            if (i == 1) {
                ILP_RETURN(SelfAwareSmall{});
            }
        }
        ILP_END_RETURN;
        return std::nullopt;
    }

    std::optional<std::uint64_t> test_ilp_for_untyped_optional_helper() {
        ILP_FOR(auto i, 0, 2, 4) {
            if (i == 1)
                ILP_RETURN(std::uint64_t{42});
        }
        ILP_END_RETURN;
        return std::nullopt;
    }
} // namespace

TEST_CASE("No copies in ILP_FOR_T macro", "[copy_count][macro]") {
    auto result = test_ilp_for_helper();
    REQUIRE(result.has_value());
    CHECK(result->value == 50);
    INFO("Copies: " << CopyMoveCounter::copies << ", Moves: " << CopyMoveCounter::moves);
    CHECK(CopyMoveCounter::copies == 0);
}

TEST_CASE("Move-only type works with ILP_FOR_T macro", "[copy_count][macro][compile-time]") {
    auto result = test_ilp_for_move_only_helper();
    REQUIRE(result.has_value());
    CHECK(result->value == 50);
}

TEST_CASE("Nested ILP_FOR_T loops move-construct self-aware small objects", "[copy_count][lifetime][nested]") {
    SelfAwareSmall::bad_move = false;

    auto result = test_ilp_for_nested_self_aware_helper();
    REQUIRE(result.has_value());
    [[maybe_unused]] SelfAwareSmall value = *std::move(result);
    CHECK_FALSE(SelfAwareSmall::bad_move);
}

TEST_CASE("ILP_FOR_T move-constructs self-aware small objects", "[copy_count][lifetime][macro]") {
    SelfAwareSmall::bad_move = false;

    auto result = test_ilp_for_self_aware_helper();
    REQUIRE(result.has_value());
    [[maybe_unused]] SelfAwareSmall value = *std::move(result);
    CHECK_FALSE(SelfAwareSmall::bad_move);
}

TEST_CASE("ILP_FOR converts an untyped result to optional", "[copy_count][macro]") {
    auto result = test_ilp_for_untyped_optional_helper();
    REQUIRE(result.has_value());
    CHECK(*result == 42);
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
