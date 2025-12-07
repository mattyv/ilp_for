#pragma once

// ILP_FOR - Compile-time loop unrolling with full control flow support
// C++23 required

// =============================================================================
// Mode Selection
// =============================================================================
// Define ILP_MODE_SIMPLE for debug mode: macros expand to plain for-loops
// with native break/continue/return. Functions (ilp::reduce, ilp::find, etc.)
// always use full ILP pattern regardless of mode.
// Default: Full ILP pattern for both macros and functions

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>

// =============================================================================
// Diagnostic helpers for better error messages
// =============================================================================
namespace ilp::detail {
    // Context tags - names appear in error messages
    struct For_Context_USE_ILP_END {};

    // Check functions - wrong context gives clear error
    constexpr void check_for_end([[maybe_unused]] For_Context_USE_ILP_END) {}
}

// CPU profile selection via -DILP_CPU=xxx
// Available: default, skylake, apple_m1, zen5
#define ILP_STRINGIFY_(x) #x
#define ILP_STRINGIFY(x) ILP_STRINGIFY_(x)
#define ILP_CPU_HEADER_(cpu) ILP_STRINGIFY(cpu_profiles / ilp_cpu_##cpu.hpp)
#define ILP_CPU_HEADER(cpu) ILP_CPU_HEADER_(cpu)

#ifdef ILP_CPU
#include ILP_CPU_HEADER(ILP_CPU)
#else
#include "cpu_profiles/ilp_cpu_default.hpp"
#endif

#include "detail/iota.hpp"
#include "detail/loops_ilp.hpp"

// =============================================================================
// Macros
// =============================================================================

#ifdef ILP_MODE_SIMPLE

#include "detail/macros_simple.hpp"

#else // !ILP_MODE_SIMPLE

// Type-erased control flow - return type deduced from function context.
// ILP_RETURN stores value in type-erased buffer, extracted via operator R().

// ----- Index-based loops -----
// ILP_FOR(loop_var, start, end, N)

#define ILP_FOR(loop_var_decl, start, end, N)                                                                          \
    if ([[maybe_unused]] auto _ilp_ret_ = [&]() -> ::ilp::ForResult { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop<N>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrl& _ilp_ctrl)

// ----- Range-based loops -----
// ILP_FOR_RANGE(loop_var, range, N)

#define ILP_FOR_RANGE(loop_var_decl, range, N)                                                                         \
    if ([[maybe_unused]] auto _ilp_ret_ = [&]() -> ::ilp::ForResult { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_range<N>(range, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrl& _ilp_ctrl)

// ----- Auto-selecting loops (use optimal N based on CPU profile) -----

#define ILP_FOR_AUTO(loop_var_decl, start, end)                                                                        \
    if ([[maybe_unused]] auto _ilp_ret_ = [&]() -> ::ilp::ForResult { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_auto(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrl& _ilp_ctrl)

#define ILP_FOR_RANGE_AUTO(loop_var_decl, range)                                                                       \
    if ([[maybe_unused]] auto _ilp_ret_ = [&]() -> ::ilp::ForResult { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_range_auto(range, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrl& _ilp_ctrl)

// ----- Loop endings -----

// Standard ending - works in void functions and when ILP_RETURN is not used
// IMPORTANT: If ILP_RETURN is used, you MUST use ILP_END_RETURN instead!
// This check runs in BOTH debug and release modes to prevent silent wrong results.
// Note: static_assert isn't possible because ILP_RETURN is often conditional at runtime
#define ILP_END );                                                                                                     \
    }                                                                                                                  \
    ();                                                                                                                \
    _ilp_ret_.has_return ? (::ilp::detail::ilp_end_with_return_error(), false) : false) {}                             \
    else(void) 0

// Use this in non-void functions when ILP_RETURN may be called
// The return value is extracted via type deduction from the function's return type
#define ILP_END_RETURN );                                                                                              \
    }                                                                                                                  \
    (); _ilp_ret_) \
    return *std::move(_ilp_ret_);                                                                                      \
    else(void) 0

// ----- Control flow -----

#define ILP_CONTINUE return

#define ILP_BREAK                                                                                                      \
    do {                                                                                                               \
        _ilp_ctrl.ok = false;                                                                                          \
        return;                                                                                                        \
    } while (0)

#define ILP_RETURN(x)                                                                                                  \
    do {                                                                                                               \
        _ilp_ctrl.storage.set(x);                                                                                      \
        _ilp_ctrl.return_set = true;                                                                                   \
        _ilp_ctrl.ok = false;                                                                                          \
        return;                                                                                                        \
    } while (0)

#endif // !ILP_MODE_SIMPLE
