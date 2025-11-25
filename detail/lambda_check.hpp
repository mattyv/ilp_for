#pragma once

#include <type_traits>
#include <concepts>

namespace ilp::detail {

// =============================================================================
// Lambda parameter type checking for performance warnings
// =============================================================================

// Helper to extract lambda parameter type
template<typename F>
struct lambda_param_type;

// Const call operator (typical for lambdas)
template<typename R, typename C, typename Arg>
struct lambda_param_type<R(C::*)(Arg) const> {
    using type = Arg;
};

// Non-const call operator
template<typename R, typename C, typename Arg>
struct lambda_param_type<R(C::*)(Arg)> {
    using type = Arg;
};

// Templated call operator (auto parameters) - extract from first instantiation
template<typename R, typename C, typename Arg, typename... Args>
struct lambda_param_type<R(C::*)(Arg, Args...) const> {
    using type = Arg;
};

template<typename R, typename C, typename Arg, typename... Args>
struct lambda_param_type<R(C::*)(Arg, Args...)> {
    using type = Arg;
};

template<typename F>
using lambda_param_t = typename lambda_param_type<decltype(&F::operator())>::type;

// =============================================================================
// Compile-time warning for by-value range parameters
// =============================================================================

// Deprecated warning function
[[deprecated("PERFORMANCE WARNING: Range loop variable should use 'auto&&' not 'auto' to avoid copying elements. "
             "This causes unnecessary copies and degrades performance. "
             "Change your loop variable from 'auto varname' to 'auto&& varname'.")]]
constexpr void warn_range_loop_copies_elements() {}

// Check if lambda parameter is by-value (not a reference)
// For range-based loops, we want to detect when users write 'auto val' instead of 'auto&& val'
template<typename F, typename ExpectedRefType>
constexpr void check_range_lambda_param() {
    // Only check if F has a simple call operator (not templated)
    // Templated lambdas (generic lambdas) can't be checked this way
    if constexpr (requires { &F::operator(); }) {
        using ActualParamType = lambda_param_t<F>;

        // Check if parameter is by-value when it should be by-reference
        // We want to warn if:
        // 1. Expected type is a reference (range elements should be accessed by reference)
        // 2. Actual parameter is NOT a reference (user wrote 'auto' instead of 'auto&&')
        if constexpr (std::is_reference_v<ExpectedRefType> && !std::is_reference_v<ActualParamType>) {
            // Issue compile-time warning
            warn_range_loop_copies_elements();
        }
    }
    // For templated/generic lambdas (auto&&), we can't easily check at compile time
    // The template parameter deduction happens later and could be value or reference
}

} // namespace ilp::detail
