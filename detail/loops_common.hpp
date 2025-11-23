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

} // namespace detail
} // namespace ilp
