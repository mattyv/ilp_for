#pragma once

// Common utilities shared across all loop implementations

#include <concepts>
#include <cstddef>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>

#include "ctrl.hpp"

// Helper for pragma stringification
#define ILP_PRAGMA_STR_(x) #x
#define ILP_PRAGMA_STR(x) ILP_PRAGMA_STR_(x)

namespace ilp {
    namespace detail {

        // =============================================================================
        // Type traits
        // =============================================================================

        // is_optional is defined in ctrl.hpp

        // Sentinel type for void loops - never triggers return, but compiles in any context
        // operator* returns void so "return *_ilp_ret_" is valid in void functions
        struct no_return_t {
            constexpr explicit operator bool() const noexcept { return false; }
            [[noreturn]] void operator*() const noexcept {
#if defined(__cpp_lib_unreachable) && __cpp_lib_unreachable >= 202202L
                std::unreachable();
#elif defined(__GNUC__) || defined(__clang__)
                __builtin_unreachable();
#elif defined(_MSC_VER)
                __assume(false);
#endif
            }
        };

        // Result type for unified for_loop: void -> no_return_t, T -> std::optional<T>
        template<typename R>
        using for_result_t = std::conditional_t<std::is_void_v<R>, no_return_t, std::optional<R>>;

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

        // Type-erased control bodies (new API without return type)
        template<typename F, typename T>
        concept ForUntypedCtrlBody = std::invocable<F, T, ForCtrl&>;

        // Range-based for loop bodies
        template<typename F, typename Ref>
        concept ForRangeBody = std::invocable<F, Ref>;

        template<typename F, typename Ref>
        concept ForRangeCtrlBody = std::invocable<F, Ref, LoopCtrl<void>&>;

        template<typename F, typename Ref, typename R>
        concept ForRangeRetBody = std::invocable<F, Ref, LoopCtrl<R>&>;

        // Type-erased range bodies (new API without return type)
        template<typename F, typename Ref>
        concept ForRangeUntypedCtrlBody = std::invocable<F, Ref, ForCtrl&>;

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
        concept PredicateBody = std::invocable<F, T> && std::same_as<std::invoke_result_t<F, T>, bool>;

        template<typename F, typename Ref>
        concept PredicateRangeBody = std::invocable<F, Ref> && std::same_as<std::invoke_result_t<F, Ref>, bool>;

        // Find range with index - body receives (value, index, end_iterator)
        template<typename F, typename Ref, typename Iter>
        concept FindRangeIdxBody = std::invocable<F, Ref, std::size_t, Iter>;

    } // namespace detail
} // namespace ilp
