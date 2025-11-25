#pragma once

// Common utilities shared across all loop implementations

#include <cstddef>
#include <optional>

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

// Check if accumulator might overflow during sum reduction
template<typename AccumT, typename ElemT>
constexpr void check_accumulator_overflow() {
    // Only warn for integral types (floating point has better overflow characteristics)
    if constexpr (std::integral<AccumT> && std::integral<ElemT>) {
        // Warn if accumulator is same size or smaller than element type
        // This is a heuristic - actual overflow depends on range size and values
        if constexpr (sizeof(AccumT) <= sizeof(ElemT)) {
            warn_accumulator_overflow<AccumT, ElemT>();
        }
        // Also warn if accumulator is only slightly larger than element
        // and both are signed (e.g., accumulating int16_t into int32_t might still overflow)
        else if constexpr (std::signed_integral<AccumT> && std::signed_integral<ElemT>) {
            if constexpr (sizeof(AccumT) == sizeof(ElemT) + sizeof(ElemT)) {
                // int32_t accumulating int16_t - warn if pattern suggests large accumulation
                // This is conservative but helps catch common mistakes
                warn_accumulator_overflow<AccumT, ElemT>();
            }
        }
    }
}

// Stricter check for when we know this is definitely a sum operation
template<typename AccumT, typename ElemT>
constexpr void check_sum_overflow() {
    if constexpr (std::integral<AccumT> && std::integral<ElemT>) {
        // For sum operations, be more aggressive with warnings
        if constexpr (sizeof(AccumT) <= sizeof(ElemT) * 2) {
            warn_accumulator_overflow<AccumT, ElemT>();
        }
    }
}

} // namespace detail
} // namespace ilp
