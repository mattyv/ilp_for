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
    // NOTE: This check is disabled because it produces false positives for generic lambdas.
    // Generic lambdas with auto&& parameters are templated and cannot be reliably checked
    // at compile time using this approach. The parameter type is only determined when the
    // lambda is instantiated, not when the containing function template is instantiated.
    //
    // Instead, users should rely on:
    // 1. Documentation clearly stating to use auto&&
    // 2. Code review and best practices
    // 3. Runtime performance testing
    //
    // Future improvement: Consider using concepts or static_assert with clearer error messages
    // that can distinguish between generic lambdas and concrete ones.

    (void)sizeof(F);  // Suppress unused parameter warning
    (void)sizeof(ExpectedRefType);
}

} // namespace ilp::detail
