#pragma once

// Common utilities shared across all loop implementations

#include <cstddef>
#include <optional>
#include <concepts>
#include <type_traits>
#include <ranges>

#include "ctrl.hpp"

// Helper for pragma stringification
#define ILP_PRAGMA_STR_(x) #x
#define ILP_PRAGMA_STR(x) ILP_PRAGMA_STR_(x)

namespace ilp {
namespace detail {

// =============================================================================
// Type traits
// =============================================================================

template<typename T>
struct is_optional : std::false_type {};

template<typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

// Result type for unified for_loop: void -> void, T -> std::optional<T>
template<typename R>
using for_result_t = std::conditional_t<std::is_void_v<R>, void, std::optional<R>>;

// =============================================================================
// Reduce result type
// =============================================================================

// Result from reduce body: value to accumulate, and whether to break early
template<typename T>
struct ReduceResult {
    T value;
    bool _break;

    constexpr ReduceResult(T v, bool b) : value(std::move(v)), _break(b) {}
    constexpr bool did_break() const { return _break; }
};

// Type trait to detect ReduceResult
template<typename T> struct is_reduce_result : std::false_type {};
template<typename T>
struct is_reduce_result<ReduceResult<T>> : std::true_type {};
template<typename T> inline constexpr bool is_reduce_result_v = is_reduce_result<T>::value;

// =============================================================================
// Compile-time validation
// =============================================================================

// Warning helper for large unroll factors
template<std::size_t N>
[[deprecated("Unroll factor N > 16 is likely counterproductive: "
             "exceeds CPU execution port throughput and causes instruction cache bloat. "
             "Typical optimal values are 4-8.")]]
constexpr void warn_large_unroll_factor() {}

template<std::size_t N>
constexpr void validate_unroll_factor() {
    static_assert(N >= 1, "Unroll factor N must be at least 1");
    if constexpr (N > 16) {
        warn_large_unroll_factor<N>();
    }
}

// =============================================================================
// Overflow risk detection
// =============================================================================

// Concept to check if a type is a signed integral type
template<typename T>
concept signed_integral_type = std::integral<T> && std::signed_integral<T>;

// Concept to check if a type is an unsigned integral type
template<typename T>
concept unsigned_integral_type = std::integral<T> && std::unsigned_integral<T>;

// Warning helper for potential overflow in reductions
template<typename AccumT, typename ElemT>
[[deprecated("Overflow risk: accumulator type may be too small for sum. "
             "Consider using a larger type (e.g., int64_t or double) or "
             "explicitly provide an init value with sufficient range. "
             "For small, bounded ranges this warning can be safely ignored.")]]
constexpr void warn_accumulator_overflow() {}

// Check for potential overflow in sum operations
template<typename AccumT, typename ElemT>
constexpr void check_sum_overflow() {
    if constexpr (std::integral<AccumT> && std::integral<ElemT>) {
        // Warn only if accumulator is smaller than element type
        // Same-size is fine - users know their data
        if constexpr (sizeof(AccumT) < sizeof(ElemT)) {
            warn_accumulator_overflow<AccumT, ElemT>();
        }
    }
}

// =============================================================================
// Body signature concepts
// =============================================================================

// Index-based for loop bodies
template<typename F, typename T>
concept ForBody = std::invocable<F, T>;

template<typename F, typename T>
concept ForCtrlBody = std::invocable<F, T, LoopCtrl<void>&>;

template<typename F, typename T, typename R>
concept ForRetBody = std::invocable<F, T, LoopCtrl<R>&>;

// Range-based for loop bodies
template<typename F, typename Ref>
concept ForRangeBody = std::invocable<F, Ref>;

template<typename F, typename Ref>
concept ForRangeCtrlBody = std::invocable<F, Ref, LoopCtrl<void>&>;

template<typename F, typename Ref, typename R>
concept ForRangeRetBody = std::invocable<F, Ref, LoopCtrl<R>&>;

// Reduce bodies - must return a value (1-arg lambdas only, ctrl is vestigial for reduce)
template<typename F, typename T>
concept ReduceBody = std::invocable<F, T> && !std::same_as<std::invoke_result_t<F, T>, void>;

template<typename F, typename Ref>
concept ReduceRangeBody = std::invocable<F, Ref> && !std::same_as<std::invoke_result_t<F, Ref>, void>;

// Find bodies - takes (index, sentinel) and returns bool, index, or optional
template<typename F, typename T>
concept FindBody = std::invocable<F, T, T>;

// Predicate bodies - simple bool return
template<typename F, typename T>
concept PredicateBody = std::invocable<F, T> &&
    std::same_as<std::invoke_result_t<F, T>, bool>;

template<typename F, typename Ref>
concept PredicateRangeBody = std::invocable<F, Ref> &&
    std::same_as<std::invoke_result_t<F, Ref>, bool>;

// Find range with index - body receives (value, index, end_iterator)
template<typename F, typename Ref, typename Iter>
concept FindRangeIdxBody = std::invocable<F, Ref, std::size_t, Iter>;

} // namespace detail
} // namespace ilp
