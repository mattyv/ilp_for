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
#define ILP_CPU_HEADER_(cpu) ILP_STRINGIFY(cpu_profiles / ilp_cpu_##cpu.hpp)
#define ILP_CPU_HEADER(cpu) ILP_CPU_HEADER_(cpu)

#ifdef ILP_CPU
#include ILP_CPU_HEADER(ILP_CPU)
#else
#include "cpu_profiles/ilp_cpu_default.hpp"
#endif

#include "detail/iota.hpp"
#include "detail/loops_ilp.hpp"

#ifdef ILP_MODE_SIMPLE

#include "detail/macros_simple.hpp"

#else // !ILP_MODE_SIMPLE

#define ILP_FOR(loop_var_decl, start, end, N)                                                                          \
    if ([[maybe_unused]] auto _ilp_ret_ = [&]() -> ::ilp::ForResult { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop<N>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrl& _ilp_ctrl)

#define ILP_FOR_RANGE(loop_var_decl, range, N)                                                                         \
    if ([[maybe_unused]] auto _ilp_ret_ = [&]() -> ::ilp::ForResult { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_range<N>(range, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrl& _ilp_ctrl)

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

// IMPORTANT: if ILP_RETURN is used, you MUST use ILP_END_RETURN instead!
#define ILP_END );                                                                                                     \
    }                                                                                                                  \
    ();                                                                                                                \
    _ilp_ret_.has_return ? (::ilp::detail::ilp_end_with_return_error(), false) : false) {}                             \
    else(void) 0

#define ILP_END_RETURN );                                                                                              \
    }                                                                                                                  \
    (); _ilp_ret_) \
    return *std::move(_ilp_ret_);                                                                                      \
    else(void) 0

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
