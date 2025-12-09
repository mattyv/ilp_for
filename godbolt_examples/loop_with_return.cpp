// ILP_FOR with ILP_RETURN - godbolt example
// Early exit loop that returns a value from the enclosing function

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

    struct SmallStorage {
        alignas(8) char buffer[8];

        template<typename T>
        [[gnu::always_inline]] inline void set(T&& val) {
            using U = std::decay_t<T>;
            static_assert(sizeof(U) <= 8, "Return type exceeds 8 bytes. Use ILP_FOR_T(type, ...) instead.");
            static_assert(alignof(U) <= 8, "Return type alignment exceeds 8. Use ILP_FOR_T(type, ...) instead.");
            new (buffer) U(static_cast<T&&>(val));
        }

        template<typename R>
        [[gnu::always_inline]] inline R extract() {
            return static_cast<R&&>(*std::launder(reinterpret_cast<R*>(buffer)));
        }
    };

    struct ForCtrl {
        bool ok = true;
        bool return_set = false;
        SmallStorage storage;
    };

    struct [[nodiscard("ILP_RETURN value ignored - did you mean ILP_END_RETURN?")]] ForResult {
        bool has_return;
        SmallStorage storage;

        explicit operator bool() const noexcept { return has_return; }

        // deduces type from function return
        struct Proxy {
            SmallStorage& s;

#if defined(_MSC_VER) && !defined(__clang__)
            // MSVC needs explicit optional handling
            template<typename R>
            inline operator R() && {
                if constexpr (detail::is_optional_v<R>) {
                    using T = typename R::value_type;
                    return R(s.template extract<T>());
                } else {
                    return s.template extract<R>();
                }
            }
#else
            // GCC/Clang do implicit Proxy→T→optional
            template<typename R>
                requires(!detail::is_optional_v<R>)
            [[gnu::always_inline]] inline operator R() && {
                return s.template extract<R>();
            }
#endif

            void operator*() && {}
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

        template<typename F, typename T>
        concept ForUntypedCtrlBody = std::invocable<F, T, ForCtrl&>;

        template<std::size_t N, std::integral T, typename F>
            requires ForUntypedCtrlBody<F, T>
        ForResult for_loop_untyped_impl(T start, T end, F&& body) {
            validate_unroll_factor<N>();
            ForCtrl ctrl;
            T i = start;

            for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
                for (std::size_t j = 0; j < N; ++j) {
                    body(i + static_cast<T>(j), ctrl);
                    if (!ctrl.ok) [[unlikely]]
                        return ForResult{ctrl.return_set, std::move(ctrl.storage)};
                }
            }

            for (; i < end; ++i) {
                body(i, ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return ForResult{ctrl.return_set, std::move(ctrl.storage)};
            }

            return ForResult{false, {}};
        }

        struct For_Context_USE_ILP_END {};
        constexpr void check_for_end([[maybe_unused]] For_Context_USE_ILP_END) {}

    } // namespace detail

    template<std::size_t N = 4, std::integral T, typename F>
        requires detail::ForUntypedCtrlBody<F, T>
    ForResult for_loop(T start, T end, F&& body) {
        return detail::for_loop_untyped_impl<N>(start, end, std::forward<F>(body));
    }

} // namespace ilp

#define ILP_FOR(loop_var_decl, start, end, N)                                                                          \
    if ([[maybe_unused]] auto _ilp_ret_ = [&]() -> ::ilp::ForResult { \
        [[maybe_unused]] auto _ilp_ctx = ::ilp::detail::For_Context_USE_ILP_END{}; \
        return ::ilp::for_loop<N>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] ::ilp::ForCtrl& _ilp_ctrl)

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
// Example: Find index and compute result, returning from function
// ============================================================================

// ILP version - uses ILP_FOR with ILP_RETURN
int find_and_square_ilp(const std::vector<int>& data, int target) {
    ILP_FOR(auto i, 0, static_cast<int>(data.size()), 4) {
        if (data[i] == target)
            ILP_RETURN(i * i);
    }
    ILP_END_RETURN;
    return -1;
}

// Hand-rolled 4x unroll
int find_and_square_handrolled(const std::vector<int>& data, int target) {
    int i = 0;
    int size = static_cast<int>(data.size());
    for (; i + 4 <= size; i += 4) {
        if (data[i] == target)
            return i * i;
        if (data[i + 1] == target)
            return (i + 1) * (i + 1);
        if (data[i + 2] == target)
            return (i + 2) * (i + 2);
        if (data[i + 3] == target)
            return (i + 3) * (i + 3);
    }
    for (; i < size; ++i) {
        if (data[i] == target)
            return i * i;
    }
    return -1;
}

// Simple loop
int find_and_square_simple(const std::vector<int>& data, int target) {
    for (int i = 0; i < static_cast<int>(data.size()); ++i) {
        if (data[i] == target)
            return i * i;
    }
    return -1;
}

int main() {
    volatile size_t n = 100;
    volatile int target = 42;
    std::vector<int> data(n);
    for (size_t i = 0; i < n; ++i) {
        data[i] = static_cast<int>(i);
    }

    int r1 = find_and_square_ilp(data, target);
    int r2 = find_and_square_handrolled(data, target);
    int r3 = find_and_square_simple(data, target);

    return (r1 == r2 && r2 == r3 && r1 == 42 * 42) ? 0 : 1;
}
