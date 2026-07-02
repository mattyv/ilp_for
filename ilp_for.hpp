// ilp_for - ILP loop unrolling for C++20
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

// CPU profile selection via -DILP_CPU_SKYLAKE, -DILP_CPU_ZEN5, etc.
#include "ilp_for/cpu_profiles/ilp_cpu.hpp"

#include "ilp_for/detail/iota.hpp"
#include "ilp_for/detail/loops_ilp.hpp"

// Fallback target for unqualified `ilp_detail_ctrl` lookup when ILP_END_RETURN is
// expanded at plain function scope - i.e. NOT nested inside another ILP_FOR body,
// where the body lambda's `ilp_detail_ctrl` parameter would otherwise shadow this.
// Declared as a function (not an object) so that shadowing it doesn't trigger
// -Wshadow/-Wshadow-all, which only cover variables/parameters/types, not ordinary
// functions. See the "Nested ILP_RETURN" design note below for how this is used.
inline void ilp_detail_ctrl() {}

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
// ILP_END vs ILP_END_RETURN, and why mismatching them is a compile error:
// The opening macro and the closing macro jointly form a single call to one of the
// ::ilp::detail::macro_for* entry points (see loops_ilp.hpp), with the closing macro appending
// the final argument: ILP_END appends ::ilp::detail::end_tag_t{}, ILP_END_RETURN appends
// ::ilp::detail::end_return_tag_t{}. Overload resolution on that tag selects which ctrl type the
// body lambda is instantiated against - EachCtrl (break-only) for ILP_END, ForCtrl/ForCtrlTyped
// (break or return) for ILP_END_RETURN. EachCtrl::return_with is poisoned with a static_assert, so
// a body that calls ILP_RETURN but is closed with ILP_END fails to compile, pointing the user at
// the fix. The ctrl lambda parameter is therefore declared `auto&`, not a concrete ctrl type, and
// the IIFE's return type is deduced rather than declared - both are required for the same lambda
// to be instantiable against either ctrl type depending on which macro_for* overload is selected.
//
// Nested ILP_RETURN propagation:
// ILP_RETURN returns from the enclosing C++ function at any nesting depth (matching
// ILP_MODE_SIMPLE, where nested loops are plain `for` loops and the inner `return` naturally
// escapes everything). This works because ILP_END_RETURN's return statement -
// `return ::ilp::detail::propagate_return(ilp_detail_ret, ilp_detail_ctrl);` - is textually
// embedded at whatever scope contains the macro invocation, and `ilp_detail_ctrl` there is looked
// up unqualified. At plain function scope, no enclosing ILP_FOR body lambda has declared that
// name, so lookup falls back to the global sentinel function `ilp_detail_ctrl()` defined above,
// and propagate_return returns the Proxy exactly as before. Nested inside another ILP_FOR's body,
// that body lambda's own ctrl parameter (named `ilp_detail_ctrl`) shadows the sentinel, so
// propagate_return sees a real ForCtrl/ForCtrlTyped<R> and stores the value into it instead,
// returning void - which lets that outer loop's own ILP_END_RETURN propagate the value one level
// further outward (or return it, if that outer loop is the outermost one). If an enclosing loop on
// the path out is closed with ILP_END instead, its ctrl is EachCtrl (break-only), and
// propagate_return hits a poisoned static_assert - the END-enforcement mechanism above extends
// transitively through nesting. The sentinel is a function specifically because shadowing a
// function does not trigger -Wshadow/-Wshadow-all, which only cover variables/parameters/types.
//
// Internal identifiers use the ilp_detail_ prefix. Identifiers containing a double underscore (and
// those beginning with an underscore followed by an uppercase letter) are reserved to the
// implementation per [lex.name]; a library is not permitted to introduce such names, so we
// deliberately avoid the double-underscore prefix while still namespacing these expansion-local
// variables to keep collisions with user code unlikely.

#define ILP_FOR(loop_var_decl, start, end, N)                                                                          \
    if ([[maybe_unused]] auto ilp_detail_ret = [&]() { \
        [[maybe_unused]] auto ilp_detail_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::detail::macro_for<N>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto& ilp_detail_ctrl)

#define ILP_FOR_RANGE(loop_var_decl, range, N)                                                                         \
    if ([[maybe_unused]] auto ilp_detail_ret = [&]() { \
        [[maybe_unused]] auto ilp_detail_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::detail::macro_for_range<N>(range, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto& ilp_detail_ctrl)

#define ILP_FOR_AUTO(loop_var_decl, start, end, loop_type, element_type)                                               \
    if ([[maybe_unused]] auto ilp_detail_ret = [&]() { \
        [[maybe_unused]] auto ilp_detail_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::detail::macro_for_auto<element_type, ::ilp::LoopType::loop_type>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto& ilp_detail_ctrl)

#define ILP_FOR_RANGE_AUTO(loop_var_decl, range, loop_type, element_type)                                              \
    if ([[maybe_unused]] auto ilp_detail_ret = [&]() { \
        [[maybe_unused]] auto ilp_detail_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::detail::macro_for_range_auto<element_type, ::ilp::LoopType::loop_type>(range, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto& ilp_detail_ctrl)

#define ILP_FOR_T(type, loop_var_decl, start, end, N)                                                                  \
    if ([[maybe_unused]] auto ilp_detail_ret = [&]() { \
        [[maybe_unused]] auto ilp_detail_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::detail::macro_for_typed<type, N>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto& ilp_detail_ctrl)

#define ILP_FOR_RANGE_T(type, loop_var_decl, range, N)                                                                 \
    if ([[maybe_unused]] auto ilp_detail_ret = [&]() { \
        [[maybe_unused]] auto ilp_detail_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::detail::macro_for_range_typed<type, N>(range, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto& ilp_detail_ctrl)

#define ILP_FOR_T_AUTO(ret_type, loop_var_decl, start, end, loop_type, element_type)                                   \
    if ([[maybe_unused]] auto ilp_detail_ret = [&]() { \
        [[maybe_unused]] auto ilp_detail_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::detail::macro_for_typed_auto<element_type, ret_type, ::ilp::LoopType::loop_type>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto& ilp_detail_ctrl)

#define ILP_FOR_RANGE_T_AUTO(ret_type, loop_var_decl, range, loop_type, element_type)                                  \
    if ([[maybe_unused]] auto ilp_detail_ret = [&]() { \
        [[maybe_unused]] auto ilp_detail_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::detail::macro_for_range_typed_auto<element_type, ret_type, ::ilp::LoopType::loop_type>(range, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto& ilp_detail_ctrl)

// A body using ILP_RETURN closed with plain ILP_END fails to compile - see the
// "ILP_END vs ILP_END_RETURN" note above.
#define ILP_END , ::ilp::detail::end_tag_t{});                                                                        \
    }                                                                                                                  \
    ();                                                                                                                \
    false) {}                                                                                                         \
    else(void) 0

#define ILP_END_RETURN , ::ilp::detail::end_return_tag_t{});                                                          \
    }                                                                                                                  \
    (); ilp_detail_ret) \
    return ::ilp::detail::propagate_return(ilp_detail_ret, ilp_detail_ctrl);                                          \
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
        ilp_detail_ctrl.break_loop();                                                                                  \
        return;                                                                                                        \
    } while (0)

#define ILP_RETURN(x)                                                                                                  \
    do {                                                                                                               \
        ilp_detail_ctrl.return_with(x);                                                                               \
        return;                                                                                                        \
    } while (0)

#endif // !ILP_MODE_SIMPLE
