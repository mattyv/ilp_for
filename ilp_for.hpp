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
#define ILP_CPU_HEADER(cpu) ILP_STRINGIFY(ilp_cpu_##cpu.hpp)

#ifdef ILP_CPU
    #include ILP_CPU_HEADER(ILP_CPU)
#else
    #include "ilp_cpu_default.hpp"
#endif

namespace ilp {

// =============================================================================
// Control Object
// =============================================================================

template<typename R = void>
struct LoopCtrl {
    bool ok = true;  // Direct bool - faster than enum comparison
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

// =============================================================================
// Index-based loops
// =============================================================================

// for_loop_simple: No control flow - maximum optimization
template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_simple(T start, T end, F&& body) {
    T i = start;

    // Unrolled loop
    for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (body(i + static_cast<T>(Is)), ...);
        }(std::make_index_sequence<N>{});
    }

    // Remainder
    for (; i < end; ++i) {
        body(i);
    }
}

// for_loop: With control flow support
template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
void for_loop(T start, T end, F&& body) {
    LoopCtrl<void> ctrl;

    T i = start;

    // Unrolled loop - lambda per iteration for better branch codegen
    for (; i + static_cast<T>(N) <= end && ctrl.ok; i += static_cast<T>(N)) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] { body(i + static_cast<T>(Is), ctrl); return ctrl.ok; }() && ...);
        }(std::make_index_sequence<N>{});
    }

    // Remainder
    for (; i < end && ctrl.ok; ++i) {
        body(i, ctrl);
    }
}

// for_loop_ret: Loop with return value
template<typename R, std::size_t N = 4, std::integral T, typename F>
std::optional<R> for_loop_ret(T start, T end, F&& body) {
    LoopCtrl<R> ctrl;

    T i = start;

    // Unrolled loop - lambda per iteration for better branch codegen
    for (; i + static_cast<T>(N) <= end && ctrl.ok; i += static_cast<T>(N)) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] { body(i + static_cast<T>(Is), ctrl); return ctrl.ok; }() && ...);
        }(std::make_index_sequence<N>{});
    }

    // Remainder
    for (; i < end && ctrl.ok; ++i) {
        body(i, ctrl);
    }

    return std::move(ctrl.return_value);
}

// for_loop_step_simple: No control flow
template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_step_simple(T start, T end, T step, F&& body) {
    T i = start;
    T last_offset = step * static_cast<T>(N - 1);

    auto in_range = [&](T val) {
        return step > 0 ? val < end : val > end;
    };

    // Unrolled loop
    while (in_range(i) && in_range(i + last_offset)) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (body(i + step * static_cast<T>(Is)), ...);
        }(std::make_index_sequence<N>{});
        i += step * static_cast<T>(N);
    }

    // Remainder
    while (in_range(i)) {
        body(i);
        i += step;
    }
}

// for_loop_step: Loop with custom step
template<std::size_t N = 4, std::integral T, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
void for_loop_step(T start, T end, T step, F&& body) {
    LoopCtrl<void> ctrl;

    T i = start;
    T last_offset = step * static_cast<T>(N - 1);

    auto in_range = [&](T val) {
        return step > 0 ? val < end : val > end;
    };

    // Unrolled loop
    while (in_range(i) && in_range(i + last_offset) && ctrl.ok) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] { body(i + step * static_cast<T>(Is), ctrl); return ctrl.ok; }() && ...);
        }(std::make_index_sequence<N>{});
        i += step * static_cast<T>(N);
    }

    // Remainder
    while (in_range(i) && ctrl.ok) {
        body(i, ctrl);
        i += step;
    }
}

// for_loop_step_ret: Loop with custom step and return value
template<typename R, std::size_t N = 4, std::integral T, typename F>
std::optional<R> for_loop_step_ret(T start, T end, T step, F&& body) {
    LoopCtrl<R> ctrl;

    T i = start;
    T last_offset = step * static_cast<T>(N - 1);

    auto in_range = [&](T val) {
        return step > 0 ? val < end : val > end;
    };

    // Unrolled loop
    while (in_range(i) && in_range(i + last_offset) && ctrl.ok) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] { body(i + step * static_cast<T>(Is), ctrl); return ctrl.ok; }() && ...);
        }(std::make_index_sequence<N>{});
        i += step * static_cast<T>(N);
    }

    // Remainder
    while (in_range(i) && ctrl.ok) {
        body(i, ctrl);
        i += step;
    }

    return std::move(ctrl.return_value);
}

// =============================================================================
// Range-based loops (random access)
// =============================================================================

// for_loop_range_simple: No control flow
template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
void for_loop_range_simple(Range&& range, F&& body) {
    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);

    std::size_t i = 0;

    // Unrolled loop
    for (; i + N <= size; i += N) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (body(it[i + Is]), ...);
        }(std::make_index_sequence<N>{});
    }

    // Remainder
    for (; i < size; ++i) {
        body(it[i]);
    }
}

template<std::size_t N = 4, std::ranges::random_access_range Range, typename F>
void for_loop_range(Range&& range, F&& body) {
    LoopCtrl<void> ctrl;

    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);

    std::size_t i = 0;

    // Unrolled loop
    for (; i + N <= size && ctrl.ok; i += N) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] { body(it[i + Is], ctrl); return ctrl.ok; }() && ...);
        }(std::make_index_sequence<N>{});
    }

    // Remainder
    for (; i < size && ctrl.ok; ++i) {
        body(it[i], ctrl);
    }
}

template<typename R, std::size_t N = 4, std::ranges::random_access_range Range, typename F>
std::optional<R> for_loop_range_ret(Range&& range, F&& body) {
    LoopCtrl<R> ctrl;

    auto it = std::ranges::begin(range);
    auto size = std::ranges::size(range);

    std::size_t i = 0;

    // Unrolled loop
    for (; i + N <= size && ctrl.ok; i += N) {
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] { body(it[i + Is], ctrl); return ctrl.ok; }() && ...);
        }(std::make_index_sequence<N>{});
    }

    // Remainder
    for (; i < size && ctrl.ok; ++i) {
        body(it[i], ctrl);
    }

    return std::move(ctrl.return_value);
}

// =============================================================================
// Auto-selecting loops (use optimal_N from CPU profile)
// =============================================================================

// Auto sum loop: uses optimal_N<LoopType::Sum, sizeof(T)>
template<std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_sum(T start, T end, F&& body) {
    constexpr std::size_t N = optimal_N<LoopType::Sum, sizeof(T)>;
    for_loop_simple<N>(start, end, std::forward<F>(body));
}

// Auto search loop: uses optimal_N<LoopType::Search, sizeof(T)>
template<typename R, std::integral T, typename F>
std::optional<R> for_loop_search(T start, T end, F&& body) {
    constexpr std::size_t N = optimal_N<LoopType::Search, sizeof(T)>;
    return for_loop_ret<R, N>(start, end, std::forward<F>(body));
}

// Auto range sum: uses optimal_N<LoopType::Sum, sizeof(element)>
template<std::ranges::random_access_range Range, typename F>
void for_loop_range_sum(Range&& range, F&& body) {
    using T = std::ranges::range_value_t<Range>;
    constexpr std::size_t N = optimal_N<LoopType::Sum, sizeof(T)>;
    for_loop_range_simple<N>(std::forward<Range>(range), std::forward<F>(body));
}

// Auto range search: uses optimal_N<LoopType::Search, sizeof(element)>
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
