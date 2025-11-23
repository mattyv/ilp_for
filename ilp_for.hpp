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
    struct ForRet_Context_USE_ILP_END_RET {};

    // Check functions - wrong context gives clear error
    constexpr void check_for_end([[maybe_unused]] For_Context_USE_ILP_END) {}
    constexpr void check_reduce_end([[maybe_unused]] Reduce_Context_USE_ILP_END_REDUCE) {}
    constexpr void check_for_ret_end([[maybe_unused]] ForRet_Context_USE_ILP_END_RET) {}
}

// CPU profile selection via -DILP_CPU=xxx
// Available: default, skylake, apple_m1
#define ILP_STRINGIFY_(x) #x
#define ILP_STRINGIFY(x) ILP_STRINGIFY_(x)
#define ILP_CPU_HEADER(cpu) ILP_STRINGIFY(cpu_profiles/ilp_cpu_##cpu.hpp)

#ifdef ILP_CPU
    #include ILP_CPU_HEADER(ILP_CPU)
#else
    #include "cpu_profiles/ilp_cpu_default.hpp"
#endif

#include "detail/loops.hpp"

// =============================================================================
// Macros
// =============================================================================

// ----- Simple loops (no control flow - maximum optimization) -----

#define ILP_FOR_SIMPLE(loop_var_name, start, end, N) \
    ::ilp::for_loop_simple<N>(start, end, [&, _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}](auto loop_var_name)

#define ILP_FOR_STEP_SIMPLE(loop_var_name, start, end, step, N) \
    ::ilp::for_loop_step_simple<N>(start, end, step, [&, _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}](auto loop_var_name)

#define ILP_FOR_RANGE_SIMPLE(loop_var_name, range, N) \
    ::ilp::for_loop_range_simple<N>(range, [&, _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}](auto&& loop_var_name)

// ----- Simple loops with return (no control flow) -----
// Returns sentinel value (end for index, end iterator for range) when not found

#define ILP_FOR_RET_SIMPLE(loop_var_name, start, end, N) \
    ::ilp::for_loop_ret_simple<N>(start, end, \
        [&, _ilp_end_ = end, _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}](auto loop_var_name)

#define ILP_FOR_STEP_RET_SIMPLE(loop_var_name, start, end, step, N) \
    ::ilp::for_loop_step_ret_simple<N>(start, end, step, \
        [&, _ilp_end_ = end, _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}](auto loop_var_name)

#define ILP_FOR_RANGE_RET_SIMPLE(loop_var_name, range, N) \
    ::ilp::for_loop_range_ret_simple<N>(range, \
        [&, _ilp_end_ = std::ranges::end(range), _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}](auto&& loop_var_name)

#define ILP_FOR_RANGE_IDX_RET_SIMPLE(loop_var_name, idx_var_name, range, N) \
    ::ilp::for_loop_range_idx_ret_simple<N>(range, \
        [&, _ilp_end_ = std::ranges::end(range), _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}](auto&& loop_var_name, [[maybe_unused]] auto idx_var_name)

// ----- Index-based loops with control flow -----

#define ILP_FOR(loop_var_name, start, end, N) \
    ::ilp::for_loop<N>(start, end, [&, _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}](auto loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_FOR_RET(ret_type, loop_var_name, start, end, N) \
    if (bool _ilp_done_ = false; !_ilp_done_) \
        for (auto _ilp_r_ = ::ilp::for_loop_ret<ret_type, N>(start, end, \
            [&, _ilp_ctx = ::ilp::detail::ForRet_Context_USE_ILP_END_RET{}](auto loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_FOR_STEP(loop_var_name, start, end, step, N) \
    ::ilp::for_loop_step<N>(start, end, step, [&, _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}](auto loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_FOR_STEP_RET(ret_type, loop_var_name, start, end, step, N) \
    if (bool _ilp_done_ = false; !_ilp_done_) \
        for (auto _ilp_r_ = ::ilp::for_loop_step_ret<ret_type, N>(start, end, step, \
            [&, _ilp_ctx = ::ilp::detail::ForRet_Context_USE_ILP_END_RET{}](auto loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

// ----- Range-based loops with control flow -----

#define ILP_FOR_RANGE(loop_var_name, range, N) \
    ::ilp::for_loop_range<N>(range, [&, _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}](auto&& loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_FOR_RANGE_RET(ret_type, loop_var_name, range, N) \
    if (bool _ilp_done_ = false; !_ilp_done_) \
        for (auto _ilp_r_ = ::ilp::for_loop_range_ret<ret_type, N>(range, \
            [&, _ilp_ctx = ::ilp::detail::ForRet_Context_USE_ILP_END_RET{}](auto&& loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

// ----- Loop endings -----

#define ILP_END )

// For control-flow RET macros (returns from enclosing function)
#define ILP_END_RET ); !_ilp_done_; _ilp_done_ = true) \
    if (_ilp_r_) return *_ilp_r_

// ----- Control flow -----

#define ILP_CONTINUE return

#define ILP_BREAK \
    do { _ilp_ctrl.ok = false; return; } while(0)

// For reduce macros - breaks and returns a value (typically the neutral element)
#define ILP_BREAK_RET(val) \
    do { _ilp_ctrl.break_loop(); return val; } while(0)

#define ILP_RETURN(x) \
    do { _ilp_ctrl.ok = false; _ilp_ctrl.return_value = x; return; } while(0)

// ----- Reduce macros (multi-accumulator ILP) -----

#define ILP_REDUCE(op, init, loop_var_name, start, end, N) \
    ::ilp::reduce<N>(start, end, init, op, [&, _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}](auto loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_REDUCE_SIMPLE(op, init, loop_var_name, start, end, N) \
    ::ilp::reduce_simple<N>(start, end, init, op, [&, _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}](auto loop_var_name)

#define ILP_REDUCE_SUM(loop_var_name, start, end, N) \
    ::ilp::reduce_sum<N>(start, end, [&, _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}](auto loop_var_name)

#define ILP_REDUCE_RANGE(op, init, loop_var_name, range, N) \
    ::ilp::reduce_range<N>(range, init, op, [&, _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}](auto&& loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_REDUCE_RANGE_SIMPLE(op, init, loop_var_name, range, N) \
    ::ilp::reduce_range_simple<N>(range, init, op, [&, _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}](auto&& loop_var_name)

#define ILP_REDUCE_RANGE_SUM(loop_var_name, range, N) \
    ::ilp::reduce_range_sum<N>(range, [&, _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}](auto&& loop_var_name)

#define ILP_REDUCE_STEP_SIMPLE(op, init, loop_var_name, start, end, step, N) \
    ::ilp::reduce_step_simple<N>(start, end, step, init, op, [&, _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}](auto loop_var_name)

#define ILP_REDUCE_STEP_SUM(loop_var_name, start, end, step, N) \
    ::ilp::reduce_step_sum<N>(start, end, step, [&, _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}](auto loop_var_name)

#define ILP_END_REDUCE \
    ; (void)::ilp::detail::check_reduce_end(_ilp_ctx); })

// ----- Auto-selecting macros (use optimal_N) -----

#define ILP_REDUCE_SUM_AUTO(loop_var_name, start, end) \
    ::ilp::reduce_sum_auto(start, end, [&, _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}](auto loop_var_name)

#define ILP_REDUCE_SIMPLE_AUTO(op, init, loop_var_name, start, end) \
    ::ilp::reduce_simple_auto(start, end, init, op, [&, _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}](auto loop_var_name)

#define ILP_REDUCE_RANGE_SUM_AUTO(loop_var_name, range) \
    ::ilp::reduce_range_sum_auto(range, [&, _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}](auto&& loop_var_name)

#define ILP_REDUCE_RANGE_SIMPLE_AUTO(op, init, loop_var_name, range) \
    ::ilp::reduce_range_simple_auto(range, init, op, [&, _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}](auto&& loop_var_name)

#define ILP_FOR_RANGE_IDX_RET_SIMPLE_AUTO(loop_var_name, idx_var_name, range) \
    ::ilp::for_loop_range_idx_ret_simple_auto(range, \
        [&, _ilp_end_ = std::ranges::end(range), _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}](auto&& loop_var_name, [[maybe_unused]] auto idx_var_name)

#define ILP_FOR_RET_SIMPLE_AUTO(loop_var_name, start, end) \
    ::ilp::for_loop_ret_simple_auto(start, end, \
        [&, _ilp_end_ = end, _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}](auto loop_var_name)
