// Comparison: Simple in-place transformation
// Demonstrates loop without control flow (SIMPLE variant)

#include <vector>
#include <cstddef>
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

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_simple_impl(T start, T end, F&& body) {
    validate_unroll_factor<N>();
    T i = start;

    // Main unrolled loop - nested loop pattern enables universal vectorization
    for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
        for (std::size_t j = 0; j < N; ++j) {
            body(i + static_cast<T>(j));
        }
    }

    for (; i < end; ++i) {
        body(i);
    }
}

struct For_Context_USE_ILP_END {};

} // namespace detail

template<std::size_t N, std::integral T, typename F>
    requires std::invocable<F, T>
void for_loop_simple(T start, T end, F&& body) {
    detail::for_loop_simple_impl<N>(start, end, std::forward<F>(body));
}

} // namespace ilp

#define ILP_FOR(ret_type, loop_var_decl, start, end, N) \
    [&]() { ::ilp::for_loop_simple<N>(start, end, [&, _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}](loop_var_decl)

#define ILP_END ); }()

// ============================================================
// ILP_FOR Version - Using public macro API
// ============================================================

void transform_ilp(std::vector<int>& data) {
    ILP_FOR(void, auto i, 0uz, data.size(), 4) {
        data[i] = data[i] * 2 + 1;
    } ILP_END;
}

// ============================================================
// Hand-rolled Version - Unrolled 4x
// ============================================================

void transform_handrolled(std::vector<int>& data) {
    size_t i = 0;
    for (; i + 4 <= data.size(); i += 4) {
        data[i] = data[i] * 2 + 1;
        data[i+1] = data[i+1] * 2 + 1;
        data[i+2] = data[i+2] * 2 + 1;
        data[i+3] = data[i+3] * 2 + 1;
    }

    // Cleanup
    for (; i < data.size(); ++i) {
        data[i] = data[i] * 2 + 1;
    }
}

// ============================================================
// Simple Version - Baseline
// ============================================================

void transform_simple(std::vector<int>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = data[i] * 2 + 1;
    }
}

// Test usage
int main() {
    volatile size_t n = 1000;

    std::vector<int> data1(n);
    std::vector<int> data2(n);
    std::vector<int> data3(n);

    for (volatile size_t i = 0; i < n; ++i) {
        data1[i] = data2[i] = data3[i] = i;
    }

    transform_ilp(data1);
    transform_handrolled(data2);
    transform_simple(data3);

    return (data1[0] == data2[0] && data2[0] == data3[0]) ? 0 : 1;
}
