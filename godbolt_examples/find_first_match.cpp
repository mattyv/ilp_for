// Comparison: Find first element matching a predicate
// Demonstrates early-exit search with ILP multi-accumulator pattern
//
// This is a self-contained example for Godbolt Compiler Explorer.
// The implementation below is extracted LINE-FOR-LINE from the ilp_for library.

#include <array>
#include <concepts>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <vector>

// =============================================================================
// Extracted from ilp_for library (LINE-FOR-LINE EXACT COPY)
// =============================================================================

namespace ilp {
    namespace detail {

        // From detail/ctrl.hpp lines 15-17:
        template<typename T>
        struct is_optional : std::false_type {};
        template<typename T>
        struct is_optional<std::optional<T>> : std::true_type {};
        template<typename T>
        inline constexpr bool is_optional_v = is_optional<T>::value;

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

        // From detail/loops_common.hpp lines 137-138:
        template<typename F, typename T>
        concept FindBody = std::invocable<F, T, T>;

        // From detail/loops_ilp.hpp lines 122-182:
        template<std::size_t N, std::integral T, typename F>
            requires FindBody<F, T>
        auto find_impl(T start, T end, F&& body) {
            validate_unroll_factor<N>();
            using R = std::invoke_result_t<F, T, T>;

            if constexpr (std::is_same_v<R, bool>) {
                // Bool mode - optimized for find, returns index
                T i = start;
                for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
                    std::array<bool, N> matches;
                    for (std::size_t j = 0; j < N; ++j) {
                        matches[j] = body(i + static_cast<T>(j), end);
                    }

                    for (std::size_t j = 0; j < N; ++j) {
                        if (matches[j])
                            return i + static_cast<T>(j);
                    }
                }
                for (; i < end; ++i) {
                    if (body(i, end))
                        return i;
                }
                return end;
            } else if constexpr (is_optional_v<R>) {
                // Optional mode - return first with value
                T i = start;
                for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
                    std::array<R, N> results;
                    for (std::size_t j = 0; j < N; ++j) {
                        results[j] = body(i + static_cast<T>(j), end);
                    }

                    for (std::size_t j = 0; j < N; ++j) {
                        if (results[j].has_value())
                            return std::move(results[j]);
                    }
                }
                for (; i < end; ++i) {
                    R result = body(i, end);
                    if (result.has_value())
                        return result;
                }
                return R{};
            } else {
                // Value mode with sentinel - returns first != end
                T i = start;
                for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
                    std::array<R, N> results;
                    for (std::size_t j = 0; j < N; ++j) {
                        results[j] = body(i + static_cast<T>(j), end);
                    }

                    for (std::size_t j = 0; j < N; ++j) {
                        if (results[j] != end)
                            return std::move(results[j]);
                    }
                }
                for (; i < end; ++i) {
                    R result = body(i, end);
                    if (result != end)
                        return result;
                }
                return static_cast<R>(end);
            }
        }

    } // namespace detail

    // From detail/loops_ilp.hpp lines 676-680:
    template<std::size_t N = 4, std::integral T, typename F>
        requires std::invocable<F, T, T>
    auto find(T start, T end, F&& body) {
        return detail::find_impl<N>(start, end, std::forward<F>(body));
    }

} // namespace ilp

// =============================================================================
// ILP Version - Using ilp::find
// =============================================================================

std::optional<size_t> find_first_above_ilp(const std::vector<int>& data, int threshold) {
    // ilp::find returns sentinel (end) when not found
    size_t result =
        ilp::find<4>(0uz, data.size(), [&](auto i, [[maybe_unused]] auto end) { return data[i] > threshold; });

    if (result == data.size()) {
        return std::nullopt;
    }
    return result;
}

// =============================================================================
// Hand-rolled Version - Sequential dependency chain
// =============================================================================

std::optional<size_t> find_first_above_handrolled(const std::vector<int>& data, int threshold) {
    size_t i = 0;
    for (; i + 4 <= data.size(); i += 4) {
        if (data[i] > threshold)
            return i;
        if (data[i + 1] > threshold)
            return i + 1;
        if (data[i + 2] > threshold)
            return i + 2;
        if (data[i + 3] > threshold)
            return i + 3;
    }
    // Cleanup loop
    for (; i < data.size(); ++i) {
        if (data[i] > threshold)
            return i;
    }
    return std::nullopt;
}

// =============================================================================
// Simple Version - Baseline
// =============================================================================

std::optional<size_t> find_first_above_simple(const std::vector<int>& data, int threshold) {
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] > threshold)
            return i;
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

    return (idx1 == idx2 && idx2 == idx3 && idx1 && *idx1 == 500) ? 0 : 1;
}
