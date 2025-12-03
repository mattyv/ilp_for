#pragma once

// ILP_FOR - Compile-time loop unrolling with full control flow support
// C++23 required

// =============================================================================
// Mode Selection
// =============================================================================
// Define one of these before including to change behavior:
//   ILP_MODE_SIMPLE  - Plain loops for testing/debugging
//   ILP_MODE_PRAGMA  - Pragma-unrolled loops (best for GCC auto-vectorization)
// Default: Full ILP pattern (best for Clang ccmp optimization)

#if defined(ILP_MODE_SIMPLE) && defined(ILP_MODE_PRAGMA)
    #error "Cannot define both ILP_MODE_SIMPLE and ILP_MODE_PRAGMA"
#endif

#include <cstddef>
#include <functional>
#include <optional>
#include <utility>
#include <concepts>
#include <ranges>
#include <type_traits>

// =============================================================================
// Diagnostic helpers for better error messages
// =============================================================================
namespace ilp::detail {
    // Context tags - names appear in error messages
    struct For_Context_USE_ILP_END {};
    struct Reduce_Context_USE_ILP_END_REDUCE {};

    // Check functions - wrong context gives clear error
    constexpr void check_for_end([[maybe_unused]] For_Context_USE_ILP_END) {}
    constexpr void check_reduce_end([[maybe_unused]] Reduce_Context_USE_ILP_END_REDUCE) {}
}

// CPU profile selection via -DILP_CPU=xxx
// Available: default, skylake, apple_m1, zen5
#define ILP_STRINGIFY_(x) #x
#define ILP_STRINGIFY(x) ILP_STRINGIFY_(x)
#define ILP_CPU_HEADER_(cpu) ILP_STRINGIFY(cpu_profiles/ilp_cpu_##cpu.hpp)
#define ILP_CPU_HEADER(cpu) ILP_CPU_HEADER_(cpu)

#ifdef ILP_CPU
    #include ILP_CPU_HEADER(ILP_CPU)
#else
    #include "cpu_profiles/ilp_cpu_default.hpp"
#endif

#include "detail/loops.hpp"

// =============================================================================
// Macros
// =============================================================================
// Unified implementations detect lambda arity at compile time.
// Compiler eliminates unused ctrl.ok checks (verified with GCC 15/Clang 21).

// ----- Index-based loops -----
// ILP_FOR(ret_type, loop_var, start, end, N)
//   ret_type = void: loop returns void
//   ret_type = T:    loop returns std::optional<T>, use ILP_RETURN(x)

#define ILP_FOR(ret_type, loop_var_decl, start, end, N) \
    [&]() -> ::ilp::detail::for_result_t<ret_type> { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop<ret_type, N>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto& _ilp_ctrl)

// ----- Range-based loops -----
// ILP_FOR_RANGE(ret_type, loop_var, range, N)
//   ret_type = void: loop returns void
//   ret_type = T:    loop returns std::optional<T>, use ILP_RETURN(x)

#define ILP_FOR_RANGE(ret_type, loop_var_decl, range, N) \
    [&]() -> ::ilp::detail::for_result_t<ret_type> { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_range<ret_type, N>(range, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto& _ilp_ctrl)

// ----- Auto-selecting loops (use optimal N based on CPU profile) -----

#define ILP_FOR_AUTO(ret_type, loop_var_decl, start, end) \
    [&]() -> ::ilp::detail::for_result_t<ret_type> { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_auto<ret_type>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_FOR_RANGE_AUTO(ret_type, loop_var_decl, range) \
    [&]() -> ::ilp::detail::for_result_t<ret_type> { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_range_auto<ret_type>(range, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto& _ilp_ctrl)

// ----- Loop endings -----

#define ILP_END ); }()

// ----- Control flow -----

#define ILP_CONTINUE return

#define ILP_BREAK \
    do { _ilp_ctrl.ok = false; return; } while(0)

// For reduce macros - return value for accumulation (fast path - uses transform_reduce)
#define ILP_REDUCE_RETURN(val) return (val)

// For reduce macros with break support - must use with ILP_REDUCE_BREAK
#define ILP_REDUCE_BREAK_VALUE(val) \
    return ::ilp::detail::ReduceResult<std::decay_t<decltype(val)>>{val, false}

// For reduce macros - break early from reduction (requires ILP_REDUCE_BREAK_VALUE for returns)
#define ILP_REDUCE_BREAK \
    return ::ilp::detail::ReduceResult<typename decltype(_ilp_reduce_type)::type>{{}, true}

#define ILP_RETURN(x) \
    do { _ilp_ctrl.ok = false; _ilp_ctrl.return_value = x; return; } while(0)

// ----- Find macros (speculative ILP - returns index/value when predicate matches) -----

#define ILP_FIND(loop_var_decl, start, end, N) \
    [&]() { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::find<N>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto _ilp_end_)

#define ILP_FIND_RANGE(loop_var_decl, range, N) \
    [&]() { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::find_range<N>(range, [&]([[maybe_unused]] loop_var_decl) -> bool

#define ILP_FIND_RANGE_AUTO(loop_var_decl, range) \
    [&]() { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::find_range_auto(range, [&]([[maybe_unused]] loop_var_decl) -> bool

#define ILP_FIND_RANGE_IDX(loop_var_decl, idx_var_decl, range, N) \
    [&]() { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::find_range_idx<N>(range, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] idx_var_decl, [[maybe_unused]] auto _ilp_end_)

// ----- Reduce macros (multi-accumulator ILP) -----
// Note: _ilp_ctrl is NOT passed to reduce lambdas - break info is in ReduceResult return type

#define ILP_REDUCE(op, init, loop_var_decl, start, end, N) \
    [&]() { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}; \
        [[maybe_unused]] auto _ilp_reduce_type = std::type_identity<std::decay_t<decltype(init)>>{}; \
        return ::ilp::reduce<N>(start, end, init, op, [&]([[maybe_unused]] loop_var_decl)

#define ILP_REDUCE_RANGE(op, init, loop_var_decl, range, N) \
    [&]() { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}; \
        [[maybe_unused]] auto _ilp_reduce_type = std::type_identity<std::decay_t<decltype(init)>>{}; \
        return ::ilp::reduce_range<N>(range, init, op, [&]([[maybe_unused]] loop_var_decl)

#define ILP_END_REDUCE ); }()

// ----- Auto-selecting macros (use optimal N based on CPU profile) -----

#define ILP_REDUCE_AUTO(op, init, loop_var_decl, start, end) \
    [&]() { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}; \
        [[maybe_unused]] auto _ilp_reduce_type = std::type_identity<std::decay_t<decltype(init)>>{}; \
        return ::ilp::reduce_auto(start, end, init, op, [&]([[maybe_unused]] loop_var_decl)

#define ILP_REDUCE_RANGE_AUTO(op, init, loop_var_decl, range) \
    [&]() { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}; \
        [[maybe_unused]] auto _ilp_reduce_type = std::type_identity<std::decay_t<decltype(init)>>{}; \
        return ::ilp::reduce_range_auto(range, init, op, [&]([[maybe_unused]] loop_var_decl)
