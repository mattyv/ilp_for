// Comparison: Sum with early exit
// Demonstrates reduce with break on condition
//
// This is a self-contained example for Godbolt Compiler Explorer.
// The implementation below is extracted LINE-FOR-LINE from the ilp_for library.

#include <vector>
#include <functional>
#include <cstddef>
#include <array>
#include <utility>
#include <type_traits>
#include <concepts>
#include <optional>

// =============================================================================
// Extracted from ilp_for library (LINE-FOR-LINE EXACT COPY)
// =============================================================================

namespace ilp {
namespace detail {

// From detail/ctrl.hpp lines 15-17:
    template<typename T> struct is_optional : std::false_type {};
    template<typename T> struct is_optional<std::optional<T>> : std::true_type {};
    template<typename T> inline constexpr bool is_optional_v = is_optional<T>::value;

// From detail/loops_ilp.hpp lines 24-36:
// Trait to detect operations with compile-time known identity elements
template<typename Op, typename T>
constexpr bool has_known_identity_v =
    std::is_same_v<std::decay_t<Op>, std::plus<>> ||
    std::is_same_v<std::decay_t<Op>, std::plus<T>> ||
    std::is_same_v<std::decay_t<Op>, std::multiplies<>> ||
    std::is_same_v<std::decay_t<Op>, std::multiplies<T>> ||
    std::is_same_v<std::decay_t<Op>, std::bit_and<>> ||
    std::is_same_v<std::decay_t<Op>, std::bit_and<T>> ||
    std::is_same_v<std::decay_t<Op>, std::bit_or<>> ||
    std::is_same_v<std::decay_t<Op>, std::bit_or<T>> ||
    std::is_same_v<std::decay_t<Op>, std::bit_xor<>> ||
    std::is_same_v<std::decay_t<Op>, std::bit_xor<T>>;

// From detail/loops_ilp.hpp lines 39-59:
template<typename Op, typename T>
constexpr T make_identity() {
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
        static_assert(has_known_identity_v<Op, T>, "make_identity requires known operation");
    }
}

// From detail/loops_ilp.hpp lines 74-90:
template<std::size_t N, typename BinaryOp, typename R, typename Init>
std::array<R, N> make_accumulators([[maybe_unused]] const BinaryOp& op, [[maybe_unused]] Init&& init) {
    if constexpr (has_known_identity_v<BinaryOp, R>) {
        // Zero-copy: directly construct identity elements via guaranteed copy elision
        return []<std::size_t... Is>(std::index_sequence<Is...>) {
            return std::array<R, N>{((void)Is, make_identity<BinaryOp, R>())...};
        }(std::make_index_sequence<N>{});
    } else {
        // Unknown ops: move first, copy rest ((N-1) copies for rvalue init)
        std::array<R, N> accs;
        accs[0] = static_cast<R>(std::forward<Init>(init));
        for (std::size_t i = 1; i < N; ++i) {
            accs[i] = accs[0];
        }
        return accs;
    }
}

// From detail/loops_common.hpp lines 51-63:
template<std::size_t N>
[[deprecated("Unroll factor N > 16 is likely counterproductive: "
             "exceeds CPU execution port throughput and causes instruction cache bloat. "
             "Typical optimal values are 4-8.")]]
constexpr void warn_large_unroll_factor() {}

template<std::size_t N>
constexpr void validate_unroll_factor() {
    static_assert(N >= 1, "Unroll factor N must be at least 1");
    if constexpr (N > 16) {
        warn_large_unroll_factor<N>();
    }
}

// From detail/loops_common.hpp line 131:
template<typename F, typename T>
concept ReduceBody = std::invocable<F, T> && !std::same_as<std::invoke_result_t<F, T>, void>;

// From detail/loops_ilp.hpp lines 425-488:
template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
    requires ReduceBody<F, T>
auto reduce_impl(T start, T end, Init&& init, BinaryOp op, F&& body) {
    validate_unroll_factor<N>();

    using ResultT = std::invoke_result_t<F, T>;

    if constexpr (is_optional_v<ResultT>) {
        // Optional path - supports early break via nullopt
        using R = typename ResultT::value_type;
        auto accs = make_accumulators<N, BinaryOp, R>(op, std::forward<Init>(init));

        T i = start;
        bool should_break = false;

        for (; i + static_cast<T>(N) <= end && !should_break; i += static_cast<T>(N)) {
            for (std::size_t j = 0; j < N && !should_break; ++j) {
                auto result = body(i + static_cast<T>(j));
                if (!result) {
                    should_break = true;
                } else {
                    accs[j] = op(accs[j], *result);
                }
            }
        }

        for (; i < end && !should_break; ++i) {
            auto result = body(i);
            if (!result) {
                should_break = true;
            } else {
                accs[0] = op(accs[0], *result);
            }
        }

        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            R out = std::forward<Init>(init);
            ((out = op(out, accs[Is])), ...);
            return out;
        }(std::make_index_sequence<N>{});
    } else {
        // Plain value path - no early break, SIMD friendly
        using R = ResultT;
        auto accs = make_accumulators<N, BinaryOp, R>(op, std::forward<Init>(init));

        T i = start;

        for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
            for (std::size_t j = 0; j < N; ++j) {
                accs[j] = op(accs[j], body(i + static_cast<T>(j)));
            }
        }

        for (; i < end; ++i) {
            accs[0] = op(accs[0], body(i));
        }

        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            R out = std::forward<Init>(init);
            ((out = op(out, accs[Is])), ...);
            return out;
        }(std::make_index_sequence<N>{});
    }
}

} // namespace detail

// From detail/loops_ilp.hpp lines 707-711:
template<std::size_t N = 4, std::integral T, typename Init, typename BinaryOp, typename F>
    requires detail::ReduceBody<F, T>
auto reduce(T start, T end, Init&& init, BinaryOp op, F&& body) {
    return detail::reduce_impl<N>(start, end, std::forward<Init>(init), op, std::forward<F>(body));
}

} // namespace ilp

// =============================================================================
// ILP Version - Using ilp::reduce with optional return for early break
// =============================================================================

int sum_until_threshold_ilp(const std::vector<int>& data, int threshold) {
    // Return std::optional<int> - nullopt triggers early break
    return ilp::reduce<4>(0uz, data.size(), 0, std::plus<>{}, [&](auto i) -> std::optional<int> {
        int val = data[i];
        if (val >= threshold) {
            return std::nullopt;  // Break
        }
        return val;
    });
}

// =============================================================================
// Hand-rolled Version
// =============================================================================

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

// =============================================================================
// Simple Version
// =============================================================================

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
    for (size_t i = 0; i < n; ++i) {
        data[i] = (i < 100) ? i : 1000;
    }

    int sum1 = sum_until_threshold_ilp(data, threshold);
    int sum2 = sum_until_threshold_handrolled(data, threshold);
    int sum3 = sum_until_threshold_simple(data, threshold);

    return (sum1 == sum2 && sum2 == sum3) ? 0 : 1;
}
