// ILP_FOR_T with large return type - godbolt example
// For return types > 8 bytes, use ILP_FOR_T(type, ...) instead of ILP_FOR

#include <concepts>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace ilp {

    namespace detail {

        template<typename T>
        struct is_optional : std::false_type {};
        template<typename T>
        struct is_optional<std::optional<T>> : std::true_type {};
        template<typename T>
        inline constexpr bool is_optional_v = is_optional<T>::value;

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

    } // namespace detail

    // Typed storage for user-specified types (exact size)
    template<typename R>
    struct TypedStorage {
        alignas(R) char buffer[sizeof(R)];

        template<typename T>
        [[gnu::always_inline]] inline void set(T&& val) {
            new (buffer) R(static_cast<T&&>(val));
        }

        [[gnu::always_inline]] inline R extract() {
            return static_cast<R&&>(*std::launder(reinterpret_cast<R*>(buffer)));
        }
    };

    // ok=false means early exit (typed version - exact size storage)
    template<typename R>
    struct ForCtrlTyped {
        bool ok = true;
        bool return_set = false;
        TypedStorage<R> storage;
    };

    // ForResult for typed version
    template<typename R>
    struct [[nodiscard("ILP_RETURN value ignored - did you mean ILP_END_RETURN?")]] ForResultTyped {
        bool has_return;
        TypedStorage<R> storage;

        explicit operator bool() const noexcept { return has_return; }

        struct Proxy {
            TypedStorage<R>& s;

            [[gnu::always_inline]] inline operator R() && { return s.extract(); }
        };

        [[gnu::always_inline]] inline Proxy operator*() { return {storage}; }
    };

    namespace detail {

        [[noreturn]] inline void ilp_end_with_return_error() {
            std::fprintf(stderr, "\n*** ILP_FOR ERROR ***\n"
                                 "ILP_RETURN was called but ILP_END was used instead of ILP_END_RETURN.\n"
                                 "The return value would be silently discarded. This is a bug.\n"
                                 "Fix: Change ILP_END to ILP_END_RETURN in the enclosing function.\n\n");
            std::abort();
        }

        template<typename F, typename T, typename R>
        concept ForTypedCtrlBody = std::invocable<F, T, ForCtrlTyped<R>&>;

        template<typename R, std::size_t N, std::integral T, typename F>
            requires ForTypedCtrlBody<F, T, R>
        ForResultTyped<R> for_loop_typed_impl(T start, T end, F&& body) {
            validate_unroll_factor<N>();
            ForCtrlTyped<R> ctrl;
            T i = start;

            for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
                for (std::size_t j = 0; j < N; ++j) {
                    body(i + static_cast<T>(j), ctrl);
                    if (!ctrl.ok) [[unlikely]]
                        return ForResultTyped<R>{ctrl.return_set, std::move(ctrl.storage)};
                }
            }

            for (; i < end; ++i) {
                body(i, ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return ForResultTyped<R>{ctrl.return_set, std::move(ctrl.storage)};
            }

            return ForResultTyped<R>{false, {}};
        }

        struct For_Context_USE_ILP_END {};
        constexpr void check_for_end([[maybe_unused]] For_Context_USE_ILP_END) {}

    } // namespace detail

    template<typename R, std::size_t N = 4, std::integral T, typename F>
        requires detail::ForTypedCtrlBody<F, T, R>
    ForResultTyped<R> for_loop_typed(T start, T end, F&& body) {
        return detail::for_loop_typed_impl<R, N>(start, end, std::forward<F>(body));
    }

} // namespace ilp

#define ILP_FOR_T(type, loop_var_decl, start, end, N)                                                                  \
    if ([[maybe_unused]] auto _ilp_ret_ = [&]() -> ::ilp::ForResultTyped<type> { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop_typed<type, N>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrlTyped<type>& _ilp_ctrl)

#define ILP_END_RETURN );                                                                                              \
    }                                                                                                                  \
    (); _ilp_ret_) \
    return *std::move(_ilp_ret_);                                                                                      \
    else(void) 0

#define ILP_RETURN(x)                                                                                                  \
    do {                                                                                                               \
        _ilp_ctrl.storage.set(x);                                                                                      \
        _ilp_ctrl.return_set = true;                                                                                   \
        _ilp_ctrl.ok = false;                                                                                          \
        return;                                                                                                        \
    } while (0)

// ============================================================================
// Example: Find matching element and return a struct with details
// ============================================================================

struct Result {
    int index;
    int value;
    int squared;
    double ratio;
};

static_assert(sizeof(Result) > 8, "Result must be > 8 bytes for this example");

// ILP version - uses ILP_FOR_T for large return type
Result find_and_compute_ilp(const std::vector<int>& data, int target) {
    ILP_FOR_T(Result, auto i, 0, static_cast<int>(data.size()), 4) {
        if (data[i] == target)
            ILP_RETURN((Result{i, data[i], data[i] * data[i], static_cast<double>(data[i]) / 100.0}));
    }
    ILP_END_RETURN;
    return Result{-1, 0, 0, 0.0};
}

// Hand-rolled 4x unroll
Result find_and_compute_handrolled(const std::vector<int>& data, int target) {
    int i = 0;
    int size = static_cast<int>(data.size());
    for (; i + 4 <= size; i += 4) {
        if (data[i] == target)
            return Result{i, data[i], data[i] * data[i], static_cast<double>(data[i]) / 100.0};
        if (data[i + 1] == target)
            return Result{i + 1, data[i + 1], data[i + 1] * data[i + 1], static_cast<double>(data[i + 1]) / 100.0};
        if (data[i + 2] == target)
            return Result{i + 2, data[i + 2], data[i + 2] * data[i + 2], static_cast<double>(data[i + 2]) / 100.0};
        if (data[i + 3] == target)
            return Result{i + 3, data[i + 3], data[i + 3] * data[i + 3], static_cast<double>(data[i + 3]) / 100.0};
    }
    for (; i < size; ++i) {
        if (data[i] == target)
            return Result{i, data[i], data[i] * data[i], static_cast<double>(data[i]) / 100.0};
    }
    return Result{-1, 0, 0, 0.0};
}

// Simple loop
Result find_and_compute_simple(const std::vector<int>& data, int target) {
    for (int i = 0; i < static_cast<int>(data.size()); ++i) {
        if (data[i] == target)
            return Result{i, data[i], data[i] * data[i], static_cast<double>(data[i]) / 100.0};
    }
    return Result{-1, 0, 0, 0.0};
}

int main() {
    volatile size_t n = 100;
    volatile int target = 42;
    std::vector<int> data(n);
    for (size_t i = 0; i < n; ++i) {
        data[i] = static_cast<int>(i);
    }

    Result r1 = find_and_compute_ilp(data, target);
    Result r2 = find_and_compute_handrolled(data, target);
    Result r3 = find_and_compute_simple(data, target);

    return (r1.index == r2.index && r2.index == r3.index && r1.index == 42) ? 0 : 1;
}
