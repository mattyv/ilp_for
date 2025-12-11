// ilp_for - ILP loop unrolling for C++23
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>

namespace ilp::detail {
    struct For_Context_USE_ILP_END {};
    constexpr void check_for_end([[maybe_unused]] For_Context_USE_ILP_END) {}
}

#define ILP_STRINGIFY_(x) #x
#define ILP_STRINGIFY(x) ILP_STRINGIFY_(x)
#define ILP_CPU_HEADER_(cpu) ILP_STRINGIFY(ilp_for / cpu_profiles / ilp_cpu_##cpu.hpp)
#define ILP_CPU_HEADER(cpu) ILP_CPU_HEADER_(cpu)

#ifdef ILP_CPU
#include ILP_CPU_HEADER(ILP_CPU)
#else
#include "ilp_for/cpu_profiles/ilp_cpu_default.hpp"
#endif

#include "ilp_for/detail/iota.hpp"
#include "ilp_for/detail/loops_ilp.hpp"

#ifdef ILP_MODE_SIMPLE

#include "ilp_for/detail/macros_simple.hpp"

#else // !ILP_MODE_SIMPLE

// Macro Design Notes:
// The ILP_FOR macro uses an if-statement with an immediately-invoked lambda to capture the user's
// loop body. This creates the syntax: ILP_FOR(...) { body } ILP_END;
//
// The semicolon placement (after ILP_END, not after the closing brace) is intentional and necessary.
// The user's { body } becomes part of the inner lambda argument to for_loop<N>(), so we cannot allow
// a semicolon after } without breaking the lambda call syntax. Alternative designs were evaluated:
//   - for-loop wrapper: cannot capture user code as callable
//   - callback pattern: requires explicit returns, loses for-loop appearance
//   - statement expressions: GCC-only, not portable
// The current if/else pattern is the most portable way to achieve for-loop syntax with early exit.
//
// Internal identifiers use double-underscore prefix (__ilp_*) which is reserved for implementation
// use per the C++ standard, minimizing collision risk with user code.

#define ILP_FOR(loop_var_decl, start, end, N)                                                                          \
    if ([[maybe_unused]] auto __ilp_ret = [&]() -> ::ilp::ForResult { \
        [[maybe_unused]] auto __ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop<N>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrl& __ilp_ctrl)

#define ILP_FOR_RANGE(loop_var_decl, range, N)                                                                         \
    if ([[maybe_unused]] auto __ilp_ret = [&]() -> ::ilp::ForResult { \
        [[maybe_unused]] auto __ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_range<N>(range, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrl& __ilp_ctrl)

#define ILP_FOR_AUTO(loop_var_decl, start, end, loop_type)                                                             \
    if ([[maybe_unused]] auto __ilp_ret = [&]() -> ::ilp::ForResult { \
        [[maybe_unused]] auto __ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_auto<::ilp::LoopType::loop_type>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrl& __ilp_ctrl)

#define ILP_FOR_RANGE_AUTO(loop_var_decl, range, loop_type)                                                            \
    if ([[maybe_unused]] auto __ilp_ret = [&]() -> ::ilp::ForResult { \
        [[maybe_unused]] auto __ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_range_auto<::ilp::LoopType::loop_type>(range, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrl& __ilp_ctrl)

#define ILP_FOR_T(type, loop_var_decl, start, end, N)                                                                  \
    if ([[maybe_unused]] auto __ilp_ret = [&]() -> ::ilp::ForResultTyped<type> { \
        [[maybe_unused]] auto __ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_typed<type, N>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrlTyped<type>& __ilp_ctrl)

#define ILP_FOR_RANGE_T(type, loop_var_decl, range, N)                                                                 \
    if ([[maybe_unused]] auto __ilp_ret = [&]() -> ::ilp::ForResultTyped<type> { \
        [[maybe_unused]] auto __ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_range_typed<type, N>(range, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrlTyped<type>& __ilp_ctrl)

#define ILP_FOR_T_AUTO(type, loop_var_decl, start, end, loop_type)                                                     \
    if ([[maybe_unused]] auto __ilp_ret = [&]() -> ::ilp::ForResultTyped<type> { \
        [[maybe_unused]] auto __ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_typed_auto<type, ::ilp::LoopType::loop_type>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrlTyped<type>& __ilp_ctrl)

#define ILP_FOR_RANGE_T_AUTO(type, loop_var_decl, range, loop_type)                                                    \
    if ([[maybe_unused]] auto __ilp_ret = [&]() -> ::ilp::ForResultTyped<type> { \
        [[maybe_unused]] auto __ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_range_typed_auto<type, ::ilp::LoopType::loop_type>(range, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrlTyped<type>& __ilp_ctrl)

// IMPORTANT: if ILP_RETURN is used, you MUST use ILP_END_RETURN instead!
#define ILP_END );                                                                                                     \
    }                                                                                                                  \
    ();                                                                                                                \
    __ilp_ret.has_return ? (::ilp::detail::ilp_end_with_return_error(), false) : false) {}                             \
    else(void) 0

#define ILP_END_RETURN );                                                                                              \
    }                                                                                                                  \
    (); __ilp_ret) \
    return *std::move(__ilp_ret);                                                                                      \
    else(void) 0

// ILP_CONTINUE returns from the loop body lambda (skips to next iteration).
// Note: In ILP_MODE_SIMPLE this maps to 'continue', but here it's 'return' from lambda.
// The do-while(0) wrapper ensures proper statement semantics in all contexts.
#define ILP_CONTINUE                                                                                                   \
    do {                                                                                                               \
        return;                                                                                                        \
    } while (0)

#define ILP_BREAK                                                                                                      \
    do {                                                                                                               \
        __ilp_ctrl.ok = false;                                                                                         \
        return;                                                                                                        \
    } while (0)

#define ILP_RETURN(x)                                                                                                  \
    do {                                                                                                               \
        __ilp_ctrl.storage.set(x);                                                                                     \
        __ilp_ctrl.return_set = true;                                                                                  \
        __ilp_ctrl.ok = false;                                                                                         \
        return;                                                                                                        \
    } while (0)

#endif // !ILP_MODE_SIMPLE
