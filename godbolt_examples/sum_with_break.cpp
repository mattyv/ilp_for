// Comparison: Sum with early exit
// Demonstrates reduce with break on condition

#include <vector>
#include <functional>
#include <cstddef>
#include <array>
#include <utility>
#include <type_traits>
#include <concepts>
#include <optional>

// Minimal ILP_FOR implementation for this example
namespace ilp {

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

namespace detail {

template<std::size_t N>
[[deprecated("Unroll factor N > 16 is likely counterproductive")]]
constexpr void warn_large_unroll_factor() {}

template<std::size_t N>
constexpr void validate_unroll_factor() {
    static_assert(N >= 1, "Unroll factor N must be at least 1");
    if constexpr (N > 16) { warn_large_unroll_factor<N>(); }
}

template<typename Op, typename T>
constexpr T operation_identity([[maybe_unused]] const Op& op, [[maybe_unused]] T init) {
    if constexpr (std::is_same_v<std::decay_t<Op>, std::plus<>> ||
                  std::is_same_v<std::decay_t<Op>, std::plus<T>>) {
        return T{};
    } else if constexpr (std::is_same_v<std::decay_t<Op>, std::multiplies<>> ||
                         std::is_same_v<std::decay_t<Op>, std::multiplies<T>>) {
        return T{1};
    } else if constexpr (std::is_same_v<std::decay_t<Op>, std::bit_and<>> ||
                         std::is_same_v<std::decay_t<Op>, std::bit_and<T>>) {
        return ~T{};  // All 1s
    } else if constexpr (std::is_same_v<std::decay_t<Op>, std::bit_or<>> ||
                         std::is_same_v<std::decay_t<Op>, std::bit_or<T>>) {
        return T{};
    } else if constexpr (std::is_same_v<std::decay_t<Op>, std::bit_xor<>> ||
                         std::is_same_v<std::decay_t<Op>, std::bit_xor<T>>) {
        return T{};
    } else {
        // For unknown operations (lambdas, etc.), fall back to using init as identity
        return init;
    }
}

template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
auto reduce_impl(T start, T end, Init init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    using R = std::invoke_result_t<F, T, LoopCtrl<void>&>;

    // Get identity element for this operation
    R identity = operation_identity(op, static_cast<R>(init));

    std::array<R, N> accs;
    accs.fill(identity);

    LoopCtrl<void> ctrl;
    T i = start;

    // Main unrolled loop - nested loop pattern enables universal vectorization
    for (; i + static_cast<T>(N) <= end && ctrl.ok; i += static_cast<T>(N)) {
        for (std::size_t j = 0; j < N && ctrl.ok; ++j) {
            accs[j] = op(accs[j], body(i + static_cast<T>(j), ctrl));
        }
    }

    // Remainder - all go to accumulator 0
    for (; i < end && ctrl.ok; ++i) {
        accs[0] = op(accs[0], body(i, ctrl));
    }

    // Final reduction - apply init exactly once (unrolled via fold expression)
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        R result = init;
        ((result = op(result, accs[Is])), ...);
        return result;
    }(std::make_index_sequence<N>{});
}

struct Reduce_Context_USE_ILP_END_REDUCE {};

} // namespace detail

template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T, LoopCtrl<void>&>
auto reduce(T start, T end, Init init, BinaryOp op, F&& body) {
    return detail::reduce_impl<N>(start, end, init, op, std::forward<F>(body));
}

} // namespace ilp

#define ILP_REDUCE(op, init, loop_var_decl, start, end, N) \
    ::ilp::reduce<N>(start, end, init, op, [&](loop_var_decl, [[maybe_unused]] auto& _ilp_ctrl)

#define ILP_BREAK_RET(val) \
    do { _ilp_ctrl.break_loop(); return val; } while(0)

#define ILP_END_REDUCE )

// ============================================================
// ILP_FOR Version - Using public macro API
// ============================================================

int sum_until_threshold_ilp(const std::vector<int>& data, int threshold) {
    return ILP_REDUCE(std::plus<>{}, 0, auto i, 0uz, data.size(), 4) {
        int val = data[i];
        if (val >= threshold) {
            ILP_BREAK_RET(0);
        }
        return val;
    } ILP_END_REDUCE;
}

// ============================================================
// Hand-rolled Version
// ============================================================

int sum_until_threshold_handrolled(const std::vector<int>& data, int threshold) {
    // 4 independent accumulators - no dependency chain!
    int sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
    size_t i = 0;

    for (; i + 4 <= data.size(); i += 4) {
        if (data[i] >= threshold) break;
        sum0 += data[i];      // Independent
        if (data[i+1] >= threshold) break;
        sum1 += data[i+1];    // Independent
        if (data[i+2] >= threshold) break;
        sum2 += data[i+2];    // Independent
        if (data[i+3] >= threshold) break;
        sum3 += data[i+3];    // Independent
    }

    // Cleanup
    for (; i < data.size(); ++i) {
        if (data[i] >= threshold) break;
        sum0 += data[i];
    }

    // Final reduction
    return sum0 + sum1 + sum2 + sum3;
}

// ============================================================
// Simple Version
// ============================================================

int sum_until_threshold_simple(const std::vector<int>& data, int threshold) {
    int sum = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] >= threshold) break;
        sum += data[i];
    }
    return sum;
}

// Test usage
int main() {
    volatile size_t n = 1000;
    volatile int threshold = 500;

    std::vector<int> data(n);
    for (volatile size_t i = 0; i < n; ++i) {
        data[i] = (i < 100) ? i : 1000;
    }

    int sum1 = sum_until_threshold_ilp(data, threshold);
    int sum2 = sum_until_threshold_handrolled(data, threshold);
    int sum3 = sum_until_threshold_simple(data, threshold);

    return (sum1 == sum2 && sum2 == sum3) ? 0 : 1;
}
