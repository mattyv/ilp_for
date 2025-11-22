#pragma once

// ILP_FOR - Compile-time loop unrolling with full control flow support
// C++23 required

#include <cstddef>
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

namespace ilp {

// =============================================================================
// Control Object
// =============================================================================

template<typename R = void>
struct LoopCtrl {
    bool ok = true;
    std::optional<R> return_value;

    void break_loop() { ok = false; }
    void return_with(R val) {
        ok = false;
        return_value = std::move(val);
    }
};

template<>
struct LoopCtrl<void> {
    bool ok = true;
    void break_loop() { ok = false; }
};

} // namespace ilp

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

// =============================================================================
// Auto-selecting loops (use optimal_N from CPU profile)
// =============================================================================

template<std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_sum(T start, T end, F&& body) {
    constexpr std::size_t N = optimal_N<LoopType::Sum, sizeof(T)>;
    for_loop_simple<N>(start, end, std::forward<F>(body));
}

template<typename R, std::integral T, typename F>
std::optional<R> for_loop_search(T start, T end, F&& body) {
    constexpr std::size_t N = optimal_N<LoopType::Search, sizeof(T)>;
    return for_loop_ret<R, N>(start, end, std::forward<F>(body));
}

template<std::ranges::random_access_range Range, typename F>
void for_loop_range_sum(Range&& range, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    constexpr std::size_t N = optimal_N<LoopType::Sum, sizeof(T)>;
    for_loop_range_simple<N>(std::forward<Range>(range), std::forward<F>(body));
}

template<typename R, std::ranges::random_access_range Range, typename F>
std::optional<R> for_loop_range_search(Range&& range, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    constexpr std::size_t N = optimal_N<LoopType::Search, sizeof(T)>;
    return for_loop_range_ret<R, N>(std::forward<Range>(range), std::forward<F>(body));
}

} // namespace ilp

// =============================================================================
// Macros
// =============================================================================

// ----- Simple loops (no control flow - maximum optimization) -----

#define ILP_FOR_SIMPLE(var, start, end, N) \
    ::ilp::for_loop_simple<N>(start, end, [&](auto var)

#define ILP_FOR_STEP_SIMPLE(var, start, end, step, N) \
    ::ilp::for_loop_step_simple<N>(start, end, step, [&](auto var)

#define ILP_FOR_RANGE_SIMPLE(var, range, N) \
    ::ilp::for_loop_range_simple<N>(range, [&](auto&& var)

// ----- Index-based loops with control flow -----

#define ILP_FOR(var, start, end, N) \
    ::ilp::for_loop<N>(start, end, [&](auto var, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_FOR_RET(ret_type, var, start, end, N) \
    if (bool _ilp_done_ = false; !_ilp_done_) \
        for (auto _ilp_r_ = ::ilp::for_loop_ret<ret_type, N>(start, end, \
            [&](auto var, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_FOR_STEP(var, start, end, step, N) \
    ::ilp::for_loop_step<N>(start, end, step, [&](auto var, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_FOR_STEP_RET(ret_type, var, start, end, step, N) \
    if (bool _ilp_done_ = false; !_ilp_done_) \
        for (auto _ilp_r_ = ::ilp::for_loop_step_ret<ret_type, N>(start, end, step, \
            [&](auto var, [[maybe_unused]] auto& _ilp_ctrl)

// ----- Range-based loops with control flow -----

#define ILP_FOR_RANGE(var, range, N) \
    ::ilp::for_loop_range<N>(range, [&](auto&& var, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_FOR_RANGE_RET(ret_type, var, range, N) \
    if (bool _ilp_done_ = false; !_ilp_done_) \
        for (auto _ilp_r_ = ::ilp::for_loop_range_ret<ret_type, N>(range, \
            [&](auto&& var, [[maybe_unused]] auto& _ilp_ctrl)

// ----- Loop endings -----

#define ILP_END )

#define ILP_END_RET ); !_ilp_done_; _ilp_done_ = true) \
    if (_ilp_r_) return *_ilp_r_

// ----- Control flow -----

#define ILP_CONTINUE return

#define ILP_BREAK \
    do { _ilp_ctrl.ok = false; return; } while(0)

#define ILP_RETURN(x) \
    do { _ilp_ctrl.ok = false; _ilp_ctrl.return_value = x; return; } while(0)
