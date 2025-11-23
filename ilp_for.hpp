#pragma once

// ILP_FOR - Compile-time loop unrolling with full control flow support
// C++23 required

#include <cstddef>
#include <functional>
#include <optional>
#include <utility>
#include <concepts>
#include <ranges>
#include <type_traits>

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

namespace ilp {

// =============================================================================
// Public API - Index-based loops
// =============================================================================

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_simple(T start, T end, F&& body) {
    detail::for_loop_simple_impl<N>(start, end, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
void for_loop(T start, T end, F&& body) {
    detail::for_loop_impl<N>(start, end, std::forward<F>(body));
}

template<typename R, std::size_t N = 4, std::integral T, typename F>
std::optional<R> for_loop_ret(T start, T end, F&& body) {
    return detail::for_loop_ret_impl<R, N>(start, end, std::forward<F>(body));
}

template<typename R, std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
std::optional<R> for_loop_ret_simple(T start, T end, F&& body) {
    return detail::for_loop_ret_simple_impl<R, N>(start, end, std::forward<F>(body));
}

// =============================================================================
// Public API - Step loops
// =============================================================================

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_step_simple(T start, T end, T step, F&& body) {
    detail::for_loop_step_simple_impl<N>(start, end, step, std::forward<F>(body));
}

template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
void for_loop_step(T start, T end, T step, F&& body) {
    detail::for_loop_step_impl<N>(start, end, step, std::forward<F>(body));
}

template<typename R, std::size_t N = 4, std::integral T, typename F>
std::optional<R> for_loop_step_ret(T start, T end, T step, F&& body) {
    return detail::for_loop_step_ret_impl<R, N>(start, end, step, std::forward<F>(body));
}

template<typename R, std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
std::optional<R> for_loop_step_ret_simple(T start, T end, T step, F&& body) {
    return detail::for_loop_step_ret_simple_impl<R, N>(start, end, step, std::forward<F>(body));
}

// =============================================================================
// Public API - Range-based loops
// =============================================================================

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
void for_loop_range_simple(Range&& range, F&& body) {
    detail::for_loop_range_simple_impl<N>(std::forward<Range>(range), std::forward<F>(body));
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
void for_loop_range(Range&& range, F&& body) {
    detail::for_loop_range_impl<N>(std::forward<Range>(range), std::forward<F>(body));
}

template<typename R, std::size_t N = 4, std::ranges::random_access_range Range, typename F>
std::optional<R> for_loop_range_ret(Range&& range, F&& body) {
    return detail::for_loop_range_ret_impl<R, N>(std::forward<Range>(range), std::forward<F>(body));
}

template<typename R, std::size_t N = 4, std::ranges::random_access_range Range, typename F>
std::optional<R> for_loop_range_ret_simple(Range&& range, F&& body) {
    return detail::for_loop_range_ret_simple_impl<R, N>(std::forward<Range>(range), std::forward<F>(body));
}

template<typename R, std::size_t N = 4, std::ranges::random_access_range Range, typename F>
std::optional<R> for_loop_range_idx_ret_simple(Range&& range, F&& body) {
    return detail::for_loop_range_idx_ret_simple_impl<N, R>(std::forward<Range>(range), std::forward<F>(body));
}

// =============================================================================
// Public API - Reduce (multi-accumulator for true ILP)
// =============================================================================

// Generic reduce with break support
template<std::size_t N = 4, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
auto reduce(T start, T end, Init init, BinaryOp op, F&& body) {
    return detail::reduce_impl<N>(start, end, init, op, std::forward<F>(body));
}

// Simple reduce (no break)
template<std::size_t N = 4, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_simple(T start, T end, Init init, BinaryOp op, F&& body) {
    return detail::reduce_simple_impl<N>(start, end, init, op, std::forward<F>(body));
}

// Convenience: reduce_sum
template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
auto reduce_sum(T start, T end, F&& body) {
    using R = std::invoke_result_t<F, T>;
    return detail::reduce_simple_impl<N>(start, end, R{}, std::plus<>{}, std::forward<F>(body));
}

// Range-based reduce
template<std::size_t N = 4, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>, LoopCtrl<void>&>
auto reduce_range(Range&& range, Init init, BinaryOp op, F&& body) {
    return detail::reduce_range_impl<N>(std::forward<Range>(range), init, op, std::forward<F>(body));
}

// Simple range reduce
template<std::size_t N = 4, std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_simple(Range&& range, Init init, BinaryOp op, F&& body) {
    return detail::reduce_range_simple_impl<N>(std::forward<Range>(range), init, op, std::forward<F>(body));
}

// Convenience: reduce_range_sum
template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_sum(Range&& range, F&& body) {
    using R = std::invoke_result_t<F, std::ranges::range_reference_t<Range>>;
    return detail::reduce_range_simple_impl<N>(std::forward<Range>(range), R{}, std::plus<>{}, std::forward<F>(body));
}

// Step-based reduce
template<std::size_t N = 4, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_step_simple(T start, T end, T step, Init init, BinaryOp op, F&& body) {
    return detail::reduce_step_simple_impl<N>(start, end, step, init, op, std::forward<F>(body));
}

// Convenience: reduce_step_sum
template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
auto reduce_step_sum(T start, T end, T step, F&& body) {
    using R = std::invoke_result_t<F, T>;
    return detail::reduce_step_simple_impl<N>(start, end, step, R{}, std::plus<>{}, std::forward<F>(body));
}

// =============================================================================
// Auto-selecting functions (use optimal_N based on element size)
// =============================================================================

// Reduce with auto N
template<std::integral T, typename F>
    requires std::invocable<F, T>
auto reduce_sum_auto(T start, T end, F&& body) {
    return reduce_sum<optimal_N<LoopType::Sum, sizeof(T)>>(start, end, std::forward<F>(body));
}

template<std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_simple_auto(T start, T end, Init init, BinaryOp op, F&& body) {
    return reduce_simple<optimal_N<LoopType::Sum, sizeof(T)>>(start, end, init, op, std::forward<F>(body));
}

// Range-based reduce with auto N
template<std::ranges::random_access_range Range, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_sum_auto(Range&& range, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    return reduce_range_sum<optimal_N<LoopType::Sum, sizeof(T)>>(std::forward<Range>(range), std::forward<F>(body));
}

template<std::ranges::random_access_range Range, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, std::ranges::range_reference_t<Range>>
auto reduce_range_simple_auto(Range&& range, Init init, BinaryOp op, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    return reduce_range_simple<optimal_N<LoopType::Sum, sizeof(T)>>(std::forward<Range>(range), init, op, std::forward<F>(body));
}

} // namespace ilp

// =============================================================================
// Macros
// =============================================================================

// ----- Simple loops (no control flow - maximum optimization) -----

#define ILP_FOR_SIMPLE(loop_var_name, start, end, N) \
    ::ilp::for_loop_simple<N>(start, end, [&](auto loop_var_name)

#define ILP_FOR_STEP_SIMPLE(loop_var_name, start, end, step, N) \
    ::ilp::for_loop_step_simple<N>(start, end, step, [&](auto loop_var_name)

#define ILP_FOR_RANGE_SIMPLE(loop_var_name, range, N) \
    ::ilp::for_loop_range_simple<N>(range, [&](auto&& loop_var_name)

// ----- Simple loops with return (no control flow) -----

#define ILP_FOR_RET_SIMPLE(ret_type, loop_var_name, start, end, N) \
    if (bool _ilp_done_ = false; !_ilp_done_) \
        for (auto _ilp_r_ = ::ilp::for_loop_ret_simple<ret_type, N>(start, end, \
            [&](auto loop_var_name) -> std::optional<ret_type>

#define ILP_FOR_STEP_RET_SIMPLE(ret_type, loop_var_name, start, end, step, N) \
    if (bool _ilp_done_ = false; !_ilp_done_) \
        for (auto _ilp_r_ = ::ilp::for_loop_step_ret_simple<ret_type, N>(start, end, step, \
            [&](auto loop_var_name) -> std::optional<ret_type>

#define ILP_FOR_RANGE_RET_SIMPLE(ret_type, loop_var_name, range, N) \
    if (bool _ilp_done_ = false; !_ilp_done_) \
        for (auto _ilp_r_ = ::ilp::for_loop_range_ret_simple<ret_type, N>(range, \
            [&](auto&& loop_var_name) -> std::optional<ret_type>

#define ILP_FOR_RANGE_IDX_RET_SIMPLE(ret_type, loop_var_name, idx_var_name, range, N) \
    if (bool _ilp_done_ = false; !_ilp_done_) \
        for (auto _ilp_r_ = ::ilp::for_loop_range_idx_ret_simple<ret_type, N>(range, \
            [&](auto&& loop_var_name, auto idx_var_name) -> std::optional<ret_type>

// ----- Index-based loops with control flow -----

#define ILP_FOR(loop_var_name, start, end, N) \
    ::ilp::for_loop<N>(start, end, [&](auto loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_FOR_RET(ret_type, loop_var_name, start, end, N) \
    if (bool _ilp_done_ = false; !_ilp_done_) \
        for (auto _ilp_r_ = ::ilp::for_loop_ret<ret_type, N>(start, end, \
            [&](auto loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_FOR_STEP(loop_var_name, start, end, step, N) \
    ::ilp::for_loop_step<N>(start, end, step, [&](auto loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_FOR_STEP_RET(ret_type, loop_var_name, start, end, step, N) \
    if (bool _ilp_done_ = false; !_ilp_done_) \
        for (auto _ilp_r_ = ::ilp::for_loop_step_ret<ret_type, N>(start, end, step, \
            [&](auto loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

// ----- Range-based loops with control flow -----

#define ILP_FOR_RANGE(loop_var_name, range, N) \
    ::ilp::for_loop_range<N>(range, [&](auto&& loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_FOR_RANGE_RET(ret_type, loop_var_name, range, N) \
    if (bool _ilp_done_ = false; !_ilp_done_) \
        for (auto _ilp_r_ = ::ilp::for_loop_range_ret<ret_type, N>(range, \
            [&](auto&& loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

// ----- Loop endings -----

#define ILP_END )

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
    ::ilp::reduce<N>(start, end, init, op, [&](auto loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_REDUCE_SIMPLE(op, init, loop_var_name, start, end, N) \
    ::ilp::reduce_simple<N>(start, end, init, op, [&](auto loop_var_name)

#define ILP_REDUCE_SUM(loop_var_name, start, end, N) \
    ::ilp::reduce_sum<N>(start, end, [&](auto loop_var_name)

#define ILP_REDUCE_RANGE(op, init, loop_var_name, range, N) \
    ::ilp::reduce_range<N>(range, init, op, [&](auto&& loop_var_name, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_REDUCE_RANGE_SIMPLE(op, init, loop_var_name, range, N) \
    ::ilp::reduce_range_simple<N>(range, init, op, [&](auto&& loop_var_name)

#define ILP_REDUCE_RANGE_SUM(loop_var_name, range, N) \
    ::ilp::reduce_range_sum<N>(range, [&](auto&& loop_var_name)

#define ILP_REDUCE_STEP_SIMPLE(op, init, loop_var_name, start, end, step, N) \
    ::ilp::reduce_step_simple<N>(start, end, step, init, op, [&](auto loop_var_name)

#define ILP_REDUCE_STEP_SUM(loop_var_name, start, end, step, N) \
    ::ilp::reduce_step_sum<N>(start, end, step, [&](auto loop_var_name)

#define ILP_END_REDUCE )

// ----- Auto-selecting macros (use optimal_N) -----

#define ILP_REDUCE_SUM_AUTO(loop_var_name, start, end) \
    ::ilp::reduce_sum_auto(start, end, [&](auto loop_var_name)

#define ILP_REDUCE_SIMPLE_AUTO(op, init, loop_var_name, start, end) \
    ::ilp::reduce_simple_auto(start, end, init, op, [&](auto loop_var_name)

#define ILP_REDUCE_RANGE_SUM_AUTO(loop_var_name, range) \
    ::ilp::reduce_range_sum_auto(range, [&](auto&& loop_var_name)

#define ILP_REDUCE_RANGE_SIMPLE_AUTO(op, init, loop_var_name, range) \
    ::ilp::reduce_range_simple_auto(range, init, op, [&](auto&& loop_var_name)
