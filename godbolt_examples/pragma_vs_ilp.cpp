// Pragma Unroll vs ILP_FOR - Godbolt example
// Shows why pragma unroll doesn't help for early-exit loops
//
// Key finding: Pragma inserts per-iteration bounds checks, ILP_FOR doesn't.
// Look for `cmp xN, xM; b.eq` after each element in the pragma version.

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

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

        struct Proxy {
            SmallStorage& s;

#if defined(_MSC_VER) && !defined(__clang__)
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

#define ILP_END );                                                                                                     \
    }                                                                                                                  \
    ();                                                                                                                \
    _ilp_ret_.has_return ? (::ilp::detail::ilp_end_with_return_error(), false) : false) {}                             \
    else(void) 0

#define ILP_CONTINUE return

#define ILP_BREAK                                                                                                      \
    do {                                                                                                               \
        _ilp_ctrl.ok = false;                                                                                          \
        return;                                                                                                        \
    } while (0)

// ============================================================================
// Example: Count elements until threshold exceeded
// ============================================================================

// Simple loop - baseline
__attribute__((noinline)) size_t count_simple(const uint32_t* data, size_t n, uint32_t threshold) {
    size_t count = 0;
    for (size_t i = 0; i < n; ++i) {
        if (data[i] > threshold)
            break;
        ++count;
    }
    return count;
}

// Pragma unroll - portable syntax for Clang/GCC
// NOTE: Look for bounds checks (cmp, b.eq) after EACH element in assembly
__attribute__((noinline)) size_t count_pragma(const uint32_t* data, size_t n, uint32_t threshold) {
    size_t count = 0;
#if defined(__clang__)
#pragma clang loop unroll_count(4)
#elif defined(__GNUC__)
#pragma GCC unroll 4
#endif
    for (size_t i = 0; i < n; ++i) {
        if (data[i] > threshold)
            break;
        ++count;
    }
    return count;
}

// ILP_FOR - bounds check only at end of each 4-element block
__attribute__((noinline)) size_t count_ilp(const uint32_t* data, size_t n, uint32_t threshold) {
    size_t count = 0;
    ILP_FOR(auto i, 0uz, n, 4) {
        if (data[i] > threshold)
            ILP_BREAK;
        ++count;
    }
    ILP_END;
    return count;
}

// ============================================================================
// Main - verify all versions produce same result
// ============================================================================

int main() {
    volatile size_t n = 100;
    uint32_t data[100];
    for (size_t i = 0; i < 100; ++i) {
        data[i] = static_cast<uint32_t>(i);
    }

    // Break at element 50
    uint32_t threshold = 49;

    size_t r1 = count_simple(data, n, threshold);
    size_t r2 = count_pragma(data, n, threshold);
    size_t r3 = count_ilp(data, n, threshold);

    return (r1 == r2 && r2 == r3 && r1 == 50) ? 0 : 1;
}
