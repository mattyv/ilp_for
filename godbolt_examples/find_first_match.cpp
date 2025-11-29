// Comparison: Find first element matching a predicate
// Demonstrates early-exit search with ILP multi-accumulator pattern

#include <vector>
#include <optional>
#include <cstddef>
#include <array>
#include <functional>
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

template<typename T>
struct is_optional : std::false_type {};

template<typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T, T>
auto find_impl(T start, T end, F&& body) {
    validate_unroll_factor<N>();
    using R = std::invoke_result_t<F, T, T>;

    if constexpr (std::is_same_v<R, bool>) {
        // Bool mode - optimized for find, returns index
        T i = start;
        for (; i + N <= end; i += N) {
            std::array<bool, N> results;
            for (std::size_t j = 0; j < N; ++j) {
                results[j] = body(i + j, end);
            }
            for (std::size_t j = 0; j < N; ++j) {
                if (results[j]) return i + j;
            }
        }
        for (; i < end; ++i) {
            if (body(i, end)) return i;
        }
        return end;
    } else if constexpr (is_optional_v<R>) {
        // Optional mode - return first with value
        T i = start;
        for (; i + N <= end; i += N) {
            std::array<R, N> results;
            for (std::size_t j = 0; j < N; ++j) {
                results[j] = body(i + j, end);
            }
            for (std::size_t j = 0; j < N; ++j) {
                if (results[j]) return results[j];
            }
        }
        for (; i < end; ++i) {
            if (auto r = body(i, end)) return r;
        }
        return std::nullopt;
    } else {
        static_assert(!sizeof(T), "Return type must be bool or std::optional<T>");
    }
}

struct For_Context_USE_ILP_END {};

} // namespace detail

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T, T>
auto find(T start, T end, F&& body) {
    return detail::find_impl<N>(start, end, std::forward<F>(body));
}

} // namespace ilp

#define ILP_FIND(loop_var_decl, start, end, N) \
    ::ilp::find<N>(start, end, \
        [&, _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}](loop_var_decl, [[maybe_unused]] auto _ilp_end_)

#define ILP_END )

// ============================================================
// ILP_FOR Version - Using public macro API
// ============================================================

std::optional<size_t> find_first_above_ilp(const std::vector<int>& data, int threshold) {
    size_t result = ILP_FIND(auto i, 0uz, data.size(), 4) {
        return data[i] > threshold;
    } ILP_END;

    // ILP_FIND returns sentinel value (end) when not found
    if (result == data.size()) {
        return std::nullopt;
    }
    return result;
}

// ============================================================
// Hand-rolled Version - Sequential dependency chain
// ============================================================

std::optional<size_t> find_first_above_handrolled(const std::vector<int>& data, int threshold) {
    size_t i = 0;
    for (; i + 4 <= data.size(); i += 4) {
        if (data[i] > threshold) return i;
        if (data[i+1] > threshold) return i+1;
        if (data[i+2] > threshold) return i+2;
        if (data[i+3] > threshold) return i+3;
    }
    // Cleanup loop
    for (; i < data.size(); ++i) {
        if (data[i] > threshold) return i;
    }
    return std::nullopt;
}

// ============================================================
// Simple Version - Baseline
// ============================================================

std::optional<size_t> find_first_above_simple(const std::vector<int>& data, int threshold) {
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] > threshold) return i;
    }
    return std::nullopt;
}

// Test usage
int main() {
    volatile size_t n = 1000;
    volatile int target_val = 100;
    volatile int threshold = 50;

    std::vector<int> data(n, 42);
    data[500] = target_val;

    auto idx1 = find_first_above_ilp(data, threshold);
    auto idx2 = find_first_above_handrolled(data, threshold);
    auto idx3 = find_first_above_simple(data, threshold);

    return (idx1 && *idx1 == 500) ? 0 : 1;
}
