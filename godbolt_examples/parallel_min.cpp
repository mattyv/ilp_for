// Comparison: Find minimum element
// Demonstrates multi-accumulator reduce pattern

#include <vector>
#include <algorithm>
#include <limits>
#include <cstddef>
#include <functional>
#include <array>
#include <utility>
#include <type_traits>
#include <concepts>

// Minimal ILP_FOR implementation for this example
namespace ilp {
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
    requires std::invocable<F, T>
auto reduce_simple_impl(T start, T end, Init init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();
    using R = std::invoke_result_t<F, T>;

    // Get identity element for this operation
    R identity = operation_identity(op, static_cast<R>(init));

    std::array<R, N> accs;
    accs.fill(identity);

    T i = start;

    // Main unrolled loop - nested loop pattern enables universal vectorization
    for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
        for (std::size_t j = 0; j < N; ++j) {
            accs[j] = op(accs[j], body(i + static_cast<T>(j)));
        }
    }

    // Remainder
    for (; i < end; ++i) {
        accs[0] = op(accs[0], body(i));
    }

    // Final reduction - apply init exactly once
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        R result = init;
        ((result = op(result, accs[Is])), ...);
        return result;
    }(std::make_index_sequence<N>{});
}

struct Reduce_Context_USE_ILP_END_REDUCE {};

} // namespace detail

template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires std::invocable<F, T>
auto reduce_simple(T start, T end, Init init, BinaryOp op, F&& body) {
    return detail::reduce_simple_impl<N>(start, end, init, op, std::forward<F>(body));
}

} // namespace ilp

#define ILP_REDUCE(op, init, loop_var_name, start, end, N) \
    ::ilp::reduce_simple<N>(start, end, init, op, [[&, _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}](auto loop_var_name), _ilp_ctx = ::ilp::detail::Reduce_Context_USE_ILP_END_REDUCE{}](loop_var_decl)

#define ILP_END_REDUCE )

// ============================================================
// ILP_FOR Version - Using public macro API
// ============================================================

int find_min_ilp(const std::vector<int>& data) {
    auto min_op = [](int a, int b) { return std::min(a, b); };

    return ILP_REDUCE(min_op, std::numeric_limits<int>::max(), auto i, 0uz, data.size(), 4) {
        return data[i];
    } ILP_END_REDUCE;
}

// ============================================================
// Hand-rolled Version - Sequential comparisons
// ============================================================

int find_min_handrolled(const std::vector<int>& data) {
    int min_val = std::numeric_limits<int>::max();
    size_t i = 0;

    for (; i + 4 <= data.size(); i += 4) {
        min_val = std::min(min_val, data[i]);
        min_val = std::min(min_val, data[i+1]);
        min_val = std::min(min_val, data[i+2]);
        min_val = std::min(min_val, data[i+3]);
    }

    // Cleanup
    for (; i < data.size(); ++i) {
        min_val = std::min(min_val, data[i]);
    }

    return min_val;
}

// ============================================================
// Simple Version - Baseline
// ============================================================

int find_min_simple(const std::vector<int>& data) {
    int min_val = std::numeric_limits<int>::max();
    for (size_t i = 0; i < data.size(); ++i) {
        min_val = std::min(min_val, data[i]);
    }
    return min_val;
}

// Test usage
int main() {
    volatile size_t n = 1000;
    std::vector<int> data(n);

    // Prevent constant propagation
    for (volatile size_t i = 0; i < n; ++i) {
        data[i] = (i * 7) % 100;
    }

    int min1 = find_min_ilp(data);
    int min2 = find_min_handrolled(data);
    int min3 = find_min_simple(data);

    return (min1 == min2 && min2 == min3) ? 0 : 1;
}
