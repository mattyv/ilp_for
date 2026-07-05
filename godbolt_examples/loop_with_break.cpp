// ILP_FOR with ILP_BREAK - godbolt example
// Early exit loop showing parallel evaluation before sequential break check

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

// Cross-platform always_inline attribute
#if defined(_MSC_VER) && !defined(__clang__)
#define ILP_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define ILP_ALWAYS_INLINE [[gnu::always_inline]] inline
#else
#define ILP_ALWAYS_INLINE inline
#endif

namespace ilp {

    namespace detail {
        // Always-false, but dependent on T so it only fires at instantiation time.
        template<typename T>
        inline constexpr bool always_false = false;

        // Tags selecting which ctrl type (and therefore which capability set) the
        // macro body lambda is instantiated against. ILP_END appends end_tag_t;
        // ILP_END_RETURN appends end_return_tag_t. See ilp_for.hpp macro layer.
        struct end_tag_t {};

        // Result of the ILP_END (no-return) macro path. Deliberately NOT
        // [[nodiscard]] - unlike ForResult/ForResultTyped, a break/continue-only
        // loop has nothing meaningful to discard.
        struct NoResult {};

    } // namespace detail

    // Selects whether the function API unrolls (Unrolled) or degrades to a plain,
    // single bounds-check-per-iteration loop (Simple). Mirrors the ILP_MODE_SIMPLE
    // macro switch, but is expressible per-call-site via an explicit template
    // argument in addition to the global default below.
    enum class Mode { Unrolled, Simple };

#ifdef ILP_MODE_SIMPLE
    inline constexpr Mode default_mode = Mode::Simple;
#else
    inline constexpr Mode default_mode = Mode::Unrolled;
#endif

    // Break-only ctrl for loops that cannot return a value: ilp::for_each,
    // ilp::for_each_range, and the macro layer's ILP_END (as opposed to
    // ILP_END_RETURN) path. return_with is poisoned - calling it is a
    // compile error, which is what turns an ILP_RETURN inside an ILP_END-closed
    // loop into a compile-time failure instead of the historical runtime abort.
    struct EachCtrl {
        bool ok = true;

        ILP_ALWAYS_INLINE void break_loop() { ok = false; }

        template<typename T>
        void return_with(T&&) {
            static_assert(detail::always_false<T>,
                          "This loop cannot return a value. "
                          "If using macros: ILP_RETURN was used inside a loop closed with ILP_END - "
                          "change ILP_END to ILP_END_RETURN in the enclosing function. "
                          "If using ilp::for_each: use ilp::for_loop instead.");
        }
    };

    namespace detail {

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

        template<typename F, typename T>
        concept ForEachBody = std::invocable<F, T, EachCtrl&>;

    } // namespace detail

// See loops_ilp.hpp: unrolled_block_end computes end - start via unsigned
// wraparound subtraction (well-defined, not UB); Clang's opt-in
// -fsanitize=unsigned-integer-overflow flags that wrap as suspicious
// regardless of intent, so it needs an explicit, scoped opt-out. GCC has no
// equivalent check.
#if defined(__clang__)
#define ILP_NO_SANITIZE_UNSIGNED_OVERFLOW __attribute__((no_sanitize("unsigned-integer-overflow")))
#else
#define ILP_NO_SANITIZE_UNSIGNED_OVERFLOW
#endif

    namespace detail {

        // Largest B with start <= B <= end and (B - start) % N == 0, computed in
        // unsigned arithmetic. The naive block-loop bound `i + N <= end` overflows
        // (UB) for signed T when end is within N of its maximum; precomputing the
        // block end and iterating `i != block_end` avoids that while keeping a
        // single comparison in the hot loop.
        template<std::size_t N, std::integral T>
        ILP_ALWAYS_INLINE constexpr T ILP_NO_SANITIZE_UNSIGNED_OVERFLOW unrolled_block_end(T start, T end) {
            using U = std::make_unsigned_t<T>;
            if (start >= end)
                return start; // empty range: block loop must not run
            const U total = static_cast<U>(end) - static_cast<U>(start);
            return static_cast<T>(static_cast<U>(start) + (total - total % static_cast<U>(N)));
        }

        // Shared Mode-aware main+remainder skeleton over an integral index range.
        // Every index-loop impl below is a thin wrapper choosing a ctrl type and
        // packaging the result; the unroll/remainder/early-exit shape lives only here.
        template<std::size_t N, Mode M, std::integral T, typename Ctrl, typename F>
        ILP_ALWAYS_INLINE void index_loop_core(T start, T end, Ctrl& ctrl, F&& body) {
            validate_unroll_factor<N>();
            T i = start;

            if constexpr (M == Mode::Unrolled) {
                const T block_end = unrolled_block_end<N>(start, end);
                for (; i != block_end; i += static_cast<T>(N)) {
                    for (std::size_t j = 0; j < N; ++j) {
                        body(i + static_cast<T>(j), ctrl);
                        if (!ctrl.ok) [[unlikely]]
                            return;
                    }
                }
            }

            for (; i < end; ++i) {
                body(i, ctrl);
                if (!ctrl.ok) [[unlikely]]
                    return;
            }
        }

        template<std::size_t N, Mode M, std::integral T, typename F>
            requires ForEachBody<F, T>
        void for_each_impl(T start, T end, F&& body) {
            EachCtrl ctrl;
            index_loop_core<N, M>(start, end, ctrl, std::forward<F>(body));
        }

        // Macro entries always use default_mode: ILP_MODE_SIMPLE only flips
        // ilp::default_mode to Mode::Simple (see mode.hpp) - the macro layer is
        // unconditional and these entry points run in both build modes - so no
        // per-call Mode plumbing is needed here.
        template<std::size_t N, typename R = void, std::integral T, typename F>
        NoResult macro_for(T start, T end, F&& body, end_tag_t) {
            for_each_impl<N, default_mode>(start, end, std::forward<F>(body));
            return {};
        }

    } // namespace detail

} // namespace ilp

#define ILP_FOR(loop_var_decl, start, end, N)                                                                          \
    if ([[maybe_unused]] auto ilp_detail_ret = [&]() { \
        return ::ilp::detail::macro_for<N>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto& ilp_detail_ctrl)

// A body using ILP_RETURN closed with plain ILP_END fails to compile - see
// ilp_for.hpp's "ILP_END vs ILP_END_RETURN" design note. (Not reachable from
// this file: it only defines the end_tag_t overload of macro_for above, since
// ILP_END_RETURN isn't used here - see godbolt_examples/INSTRUCTIONS.md.)
#define ILP_END , ::ilp::detail::end_tag_t{});                                                                        \
    }                                                                                                                  \
    ();                                                                                                                \
    false) {}                                                                                                         \
    else(void) 0

// ILP_CONTINUE returns from the loop body lambda (skips to next iteration).
// The do-while(0) wrapper ensures proper statement semantics in all contexts.
#define ILP_CONTINUE                                                                                                   \
    do {                                                                                                               \
        return;                                                                                                        \
    } while (0)

#define ILP_BREAK                                                                                                      \
    do {                                                                                                               \
        ilp_detail_ctrl.break_loop();                                                                                  \
        return;                                                                                                        \
    } while (0)

// ============================================================================
// Example: Process elements until negative value found
// ============================================================================

// ILP version - uses ILP_FOR with ILP_BREAK
void process_until_negative_ilp(const std::vector<int>& data, std::vector<int>& out) {
    ILP_FOR(auto i, std::size_t{0}, data.size(), 4) {
        if (data[i] < 0)
            ILP_BREAK;
        out.push_back(data[i] * 2);
    }
    ILP_END;
}

// Hand-rolled 4x unroll
void process_until_negative_handrolled(const std::vector<int>& data, std::vector<int>& out) {
    size_t i = 0;
    for (; i + 4 <= data.size(); i += 4) {
        if (data[i] < 0)
            return;
        out.push_back(data[i] * 2);
        if (data[i + 1] < 0)
            return;
        out.push_back(data[i + 1] * 2);
        if (data[i + 2] < 0)
            return;
        out.push_back(data[i + 2] * 2);
        if (data[i + 3] < 0)
            return;
        out.push_back(data[i + 3] * 2);
    }
    for (; i < data.size(); ++i) {
        if (data[i] < 0)
            return;
        out.push_back(data[i] * 2);
    }
}

// Simple loop
void process_until_negative_simple(const std::vector<int>& data, std::vector<int>& out) {
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] < 0)
            break;
        out.push_back(data[i] * 2);
    }
}

int main() {
    volatile size_t n = 100;
    std::vector<int> data(n);
    for (size_t i = 0; i < n; ++i) {
        data[i] = (i < 50) ? static_cast<int>(i) : -1;
    }

    std::vector<int> out1, out2, out3;
    process_until_negative_ilp(data, out1);
    process_until_negative_handrolled(data, out2);
    process_until_negative_simple(data, out3);

    return (out1.size() == out2.size() && out2.size() == out3.size()) ? 0 : 1;
}
