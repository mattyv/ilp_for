// ILP_FOR with ILP_RETURN - godbolt example
// Early exit loop that returns a value from the enclosing function

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <optional>
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

// Debug-mode stored-vs-recovered type check for the type-erased SBO path
// (DESIGN_NOTES.md item 3). Enabled by default whenever NDEBUG is not defined
// (assert-style); force-enable with ILP_DEBUG_TYPECHECK, force-disable with
// ILP_NO_DEBUG_TYPECHECK. Zero members / zero code when disabled.
#if !defined(ILP_NO_DEBUG_TYPECHECK) && (defined(ILP_DEBUG_TYPECHECK) || !defined(NDEBUG))
#define ILP_TYPECHECK_ENABLED 1
#include <cstdio>
#include <cstdlib>
#else
#define ILP_TYPECHECK_ENABLED 0
#endif

namespace ilp::arch {

    /// Size of the largest integral type on this architecture.
    /// Typically 8 bytes on 64-bit platforms, 4 bytes on 32-bit.
    inline constexpr std::size_t max_integral_size = sizeof(std::intmax_t);

/// SBO buffer size for SmallStorage. Defaults to max integral size.
/// Override with -DILP_SBO_SIZE=N if needed.
#ifdef ILP_SBO_SIZE
    inline constexpr std::size_t sbo_size = ILP_SBO_SIZE;
#else
    inline constexpr std::size_t sbo_size = max_integral_size;
#endif

} // namespace ilp::arch

namespace ilp {

    namespace detail {
        template<typename T>
        struct is_optional : std::false_type {};
        template<typename T>
        struct is_optional<std::optional<T>> : std::true_type {};
        template<typename T>
        inline constexpr bool is_optional_v = is_optional<T>::value;

        // Detects ForResult::Proxy / ForResultTyped<R>::Proxy, which tag themselves
        // with a nested `ilp_is_proxy` typedef. Used to reject storing a Proxy in
        // SmallStorage/TypedStorage: a Proxy wraps a reference to storage that is
        // about to go out of scope, so storing it would store a dangling reference.
        template<typename T, typename = void>
        struct is_proxy : std::false_type {};
        template<typename T>
        struct is_proxy<T, std::void_t<typename T::ilp_is_proxy>> : std::true_type {};
        template<typename T>
        inline constexpr bool is_proxy_v = is_proxy<T>::value;

        // Always-false, but dependent on T so it only fires at instantiation time.
        template<typename T>
        inline constexpr bool always_false = false;

#if ILP_TYPECHECK_ENABLED
        // RTTI-free type identity for the debug-mode SBO type check. Compare the
        // compiler signature text rather than inline-variable addresses so correct
        // types also match across shared-library/hidden-visibility boundaries.
        template<typename T>
        constexpr const char* type_name() {
#if defined(_MSC_VER) && !defined(__clang__)
            return __FUNCSIG__;
#else
            return __PRETTY_FUNCTION__;
#endif
        }

        struct TypeTag {
            const char* name;
        };

        template<typename T>
        inline constexpr TypeTag type_tag_v{type_name<T>()};

        [[noreturn]] inline void type_mismatch_abort(const char* stored, const char* recovered) {
            std::fprintf(stderr,
                         "\n*** ilp_for type mismatch ***\n"
                         "ILP_RETURN (or return_with) stored a value of type\n    %s\n"
                         "but it is being recovered as\n    %s\n"
                         "The untyped SBO path requires these to match exactly "
                         "(docs/DESIGN_NOTES.md item 3).\n"
                         "Fix: make the returned expression's type match the enclosing "
                         "function/loop return type exactly, or use ILP_FOR_T / "
                         "for_loop_typed with an explicit type.\n\n",
                         stored, recovered);
            std::abort();
        }

        // A value-bearing result Proxy must be converted exactly once. Destroying
        // one unconverted means the caller wrote a discard such as `*result;`
        // instead of extracting or returning the value.
        [[noreturn]] inline void swallowed_proxy_abort() {
            std::fprintf(stderr,
                         "\n*** ilp_for swallowed return value ***\n"
                         "A loop result holding a value was destroyed without being converted.\n"
                         "A value-bearing result Proxy was discarded instead of converted.\n"
                         "Fix: extract it into a typed local or return it from the enclosing function.\n\n");
            std::abort();
        }
#endif // ILP_TYPECHECK_ENABLED

        // Tags selecting which ctrl type (and therefore which capability set) the
        // macro body lambda is instantiated against. ILP_END appends end_tag_t;
        // ILP_END_RETURN appends end_return_tag_t. See ilp_for.hpp macro layer.
        struct end_tag_t {};
        struct end_return_tag_t {};

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

    // Small buffer optimization for integral and other small, trivially-copyable types.
    // Buffer size matches the largest integral type on this architecture (typically 8 bytes).
    // Byte transport is valid only for trivially-copyable objects.
    // Use ILP_FOR_T for non-trivial or larger return types.
    struct SmallStorage {
        alignas(arch::sbo_size) unsigned char buffer[arch::sbo_size]{};
#if ILP_TYPECHECK_ENABLED
        const detail::TypeTag* ilp_debug_stored_tag = nullptr;
#endif

        SmallStorage() = default;
        SmallStorage(const SmallStorage&) = delete;
        SmallStorage& operator=(const SmallStorage&) = delete;

        ILP_ALWAYS_INLINE SmallStorage(SmallStorage&& other) noexcept { move_from(other); }

        ILP_ALWAYS_INLINE SmallStorage& operator=(SmallStorage&& other) noexcept {
            if (this != &other)
                move_from(other);
            return *this;
        }

        template<typename T>
        ILP_ALWAYS_INLINE void set(T&& val) {
            using U = std::decay_t<T>;
            static_assert(!detail::is_proxy_v<U>,
                          "Cannot store a loop-result Proxy: extract the value into a typed local "
                          "first, or return it directly with `return *std::move(r);`.");
            static_assert(sizeof(U) <= arch::sbo_size,
                          "Return type exceeds SBO size. Use ILP_FOR_T(type, ...) instead.");
            static_assert(alignof(U) <= arch::sbo_size,
                          "Return type alignment exceeds SBO size. Use ILP_FOR_T(type, ...) instead.");
            static_assert(std::is_trivially_copyable_v<U>,
                          "SmallStorage only supports trivially-copyable types. "
                          "Use ILP_FOR_T(type, ...) or for_loop_typed for this return type.");
            new (buffer) U(static_cast<T&&>(val));
#if ILP_TYPECHECK_ENABLED
            ilp_debug_stored_tag = &detail::type_tag_v<U>;
#endif
        }

        template<typename R>
        ILP_ALWAYS_INLINE R extract() {
            using Rt = std::remove_cvref_t<R>;
            static_assert(!std::is_reference_v<R>,
                          "SmallStorage cannot return references to its internal buffer. "
                          "Return a value or use an explicitly owned typed result.");
            static_assert(sizeof(Rt) <= arch::sbo_size,
                          "Recovered type exceeds SBO size. Use ILP_FOR_T(type, ...) instead.");
            static_assert(std::is_trivially_copyable_v<Rt>,
                          "SmallStorage can only recover trivially-copyable types. "
                          "Use ILP_FOR_T(type, ...) or for_loop_typed for this return type.");
#if ILP_TYPECHECK_ENABLED
            if (ilp_debug_stored_tag != nullptr &&
                std::strcmp(ilp_debug_stored_tag->name, detail::type_tag_v<Rt>.name) != 0)
                detail::type_mismatch_abort(ilp_debug_stored_tag->name, detail::type_tag_v<Rt>.name);
#endif
            std::array<std::byte, sizeof(Rt)> representation{};
            std::memcpy(representation.data(), buffer, sizeof(Rt));
            Rt result = std::bit_cast<Rt>(representation);
#if ILP_TYPECHECK_ENABLED
            ilp_debug_stored_tag = nullptr;
#endif
            return result;
        }

    private:
        ILP_ALWAYS_INLINE void move_from(SmallStorage& other) noexcept {
            std::memcpy(buffer, other.buffer, sizeof(buffer));
#if ILP_TYPECHECK_ENABLED
            ilp_debug_stored_tag = other.ilp_debug_stored_tag;
            other.ilp_debug_stored_tag = nullptr;
#endif
        }
    };

    // Typed storage for user-specified types. std::optional owns the lifetime so
    // stored objects are destroyed on extraction, replacement, or abandonment.
    template<typename R>
    struct TypedStorage {
        std::optional<R> value;

        template<typename T>
        ILP_ALWAYS_INLINE void set(T&& val) {
            static_assert(!detail::is_proxy_v<std::decay_t<T>>,
                          "Cannot store a loop-result Proxy: extract the value into a typed local "
                          "first, or return it directly with `return *std::move(r);`.");
            value.emplace(static_cast<T&&>(val));
        }

        ILP_ALWAYS_INLINE R extract() {
            R tmp = static_cast<R&&>(*value);
            value.reset();
            return tmp;
        }

        ILP_ALWAYS_INLINE std::optional<R> extract_optional() {
            std::optional<R> result(std::in_place, static_cast<R&&>(*value));
            value.reset();
            return result;
        }

        ILP_ALWAYS_INLINE void move_from(TypedStorage& other) {
            value.emplace(static_cast<R&&>(*other.value));
            other.value.reset();
        }
    };

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

    // ok=false means early exit (simple version - 8-byte storage)
    struct ForCtrl {
        bool ok = true;
        bool return_set = false;
        SmallStorage storage;

        // Equivalent to ILP_BREAK. Call as `return ctrl.break_loop();` to break
        // and exit the body in one statement.
        ILP_ALWAYS_INLINE void break_loop() { ok = false; }

        // Equivalent to ILP_RETURN(val). Call as `return ctrl.return_with(val);`.
        template<typename T>
        ILP_ALWAYS_INLINE void return_with(T&& val) {
            storage.set(static_cast<T&&>(val));
            return_set = true;
            ok = false;
        }
    };

    // ok=false means early exit (typed version - explicit return type)
    template<typename R>
    struct ForCtrlTyped {
        bool ok = true;
        bool return_set = false;
        TypedStorage<R> storage;

        // Equivalent to ILP_BREAK. Call as `return ctrl.break_loop();` to break
        // and exit the body in one statement.
        ILP_ALWAYS_INLINE void break_loop() { ok = false; }

        // Equivalent to ILP_RETURN(val). Call as `return ctrl.return_with(val);`.
        template<typename T>
        ILP_ALWAYS_INLINE void return_with(T&& val) {
            storage.set(static_cast<T&&>(val));
            return_set = true;
            ok = false;
        }
    };

    struct [[nodiscard("ILP_RETURN value ignored - did you mean ILP_END_RETURN?")]] ForResult {
        bool has_return;
        SmallStorage storage;

        explicit operator bool() const noexcept { return has_return; }

        // deduces type from function return
        struct Proxy {
            using ilp_is_proxy = void; // tag detected by detail::is_proxy_v
            SmallStorage& s;
#if ILP_TYPECHECK_ENABLED
            // DESIGN_NOTES.md item 5: every conversion path below marks the Proxy
            // consumed; one destroyed with a value PRESENT but never converted is
            // the signature of a swallowed return value (see swallowed_proxy_abort).
            // An empty result's Proxy (has_value false) may be discarded freely -
            // there is nothing to swallow.
            bool ilp_debug_has_value = false;
            mutable bool ilp_debug_consumed = false;

            ILP_ALWAYS_INLINE Proxy(SmallStorage& storage, bool has_value) noexcept
                : s(storage), ilp_debug_has_value(has_value) {}
            // Copying transfers the consume obligation to the copy (relevant on
            // MSVC, whose conversions are lvalue-callable, so proxies can be named
            // and copied): the source counts as consumed by its copy.
            ILP_ALWAYS_INLINE Proxy(const Proxy& other) noexcept
                : s(other.s), ilp_debug_has_value(other.ilp_debug_has_value) {
                other.ilp_debug_consumed = true;
            }
            ~Proxy() {
                if (ilp_debug_has_value && !ilp_debug_consumed)
                    detail::swallowed_proxy_abort();
            }
#endif
            // No-op unless ILP_TYPECHECK_ENABLED; defined once so the conversion
            // operators below don't each need their own #if block.
            ILP_ALWAYS_INLINE void ilp_debug_mark_consumed() const noexcept {
#if ILP_TYPECHECK_ENABLED
                ilp_debug_consumed = true;
#endif
            }

#if defined(_MSC_VER) && !defined(__clang__)
            // MSVC needs explicit overloads without && qualifier
            // templated conversion operators don't deduce properly in return statements
            template<typename T>
            operator std::optional<T>() {
                ilp_debug_mark_consumed();
                return std::optional<T>(s.template extract<T>());
            }

            template<typename R, std::enable_if_t<!detail::is_optional_v<R>, int> = 0>
            operator R() {
                ilp_debug_mark_consumed();
                return s.template extract<R>();
            }
#else
            template<typename R>
                requires(!detail::is_optional_v<R>)
            ILP_ALWAYS_INLINE operator R() && {
                ilp_debug_mark_consumed();
                return s.template extract<R>();
            }
#endif

            void operator*() && { ilp_debug_mark_consumed(); }
        };

#if ILP_TYPECHECK_ENABLED
        ILP_ALWAYS_INLINE Proxy operator*() { return Proxy{storage, has_return}; }
#else
        ILP_ALWAYS_INLINE Proxy operator*() { return {storage}; }
#endif
    };

    // ForResult for typed version
    template<typename R>
    struct [[nodiscard("ILP_RETURN value ignored - did you mean ILP_END_RETURN?")]] ForResultTyped {
        bool has_return;
        TypedStorage<R> storage;

        explicit operator bool() const noexcept { return has_return; }

        // Proxy for consistency with ForResult (type is known here)
        struct Proxy {
            using ilp_is_proxy = void; // tag detected by detail::is_proxy_v
            TypedStorage<R>& s;
#if ILP_TYPECHECK_ENABLED
            // See ForResult::Proxy above (DESIGN_NOTES.md item 5) for rationale.
            bool ilp_debug_has_value = false;
            mutable bool ilp_debug_consumed = false;

            ILP_ALWAYS_INLINE Proxy(TypedStorage<R>& storage, bool has_value) noexcept
                : s(storage), ilp_debug_has_value(has_value) {}
            ILP_ALWAYS_INLINE Proxy(const Proxy& other) noexcept
                : s(other.s), ilp_debug_has_value(other.ilp_debug_has_value) {
                other.ilp_debug_consumed = true;
            }
            ~Proxy() {
                if (ilp_debug_has_value && !ilp_debug_consumed)
                    detail::swallowed_proxy_abort();
            }
#endif
            // No-op unless ILP_TYPECHECK_ENABLED (see ForResult::Proxy above).
            ILP_ALWAYS_INLINE void ilp_debug_mark_consumed() const noexcept {
#if ILP_TYPECHECK_ENABLED
                ilp_debug_consumed = true;
#endif
            }

#if defined(_MSC_VER) && !defined(__clang__)
            operator std::optional<R>() {
                ilp_debug_mark_consumed();
                return s.extract_optional();
            }

            operator R() {
                ilp_debug_mark_consumed();
                return s.extract();
            }
#else
            ILP_ALWAYS_INLINE operator std::optional<R>() && {
                ilp_debug_mark_consumed();
                return s.extract_optional();
            }

            ILP_ALWAYS_INLINE operator R() && {
                ilp_debug_mark_consumed();
                return s.extract();
            }
#endif
        };

#if ILP_TYPECHECK_ENABLED
        ILP_ALWAYS_INLINE Proxy operator*() { return Proxy{storage, has_return}; }
#else
        ILP_ALWAYS_INLINE Proxy operator*() { return {storage}; }
#endif
    };

    namespace detail {

        // Maps a ctrl type to the return type of its enclosing typed loop, if any.
        // Used by propagate_return to decide how to feed an inner loop's result into
        // an outer ForCtrlTyped<R>.
        template<typename T>
        struct ctrl_typed_return {
            using type = void;
            static constexpr bool is_typed = false;
        };
        template<typename R>
        struct ctrl_typed_return<ForCtrlTyped<R>> {
            using type = R;
            static constexpr bool is_typed = true;
        };

        // Dispatch target for ILP_END_RETURN. `outer` is whatever unqualified lookup
        // finds for `ilp_detail_ctrl` at the ILP_END_RETURN expansion point:
        //  - the global sentinel function (ilp_detail_ctrl() in ilp_for.hpp) -> this
        //    ILP_END_RETURN is at plain function scope (not nested): return the Proxy,
        //    exactly as before.
        //  - ForCtrl / ForCtrlTyped<R> -> nested inside an outer ILP_END_RETURN loop:
        //    carry the value into the outer loop's ctrl and return void, so the outer
        //    loop's own ILP_END_RETURN propagates it one level further (or returns it,
        //    if the outer loop is the outermost one).
        //  - EachCtrl -> nested inside an outer ILP_END (break-only) loop: the value
        //    has nowhere to go, so this is a compile error naming the fix.
        template<typename Res, typename Ctrl>
        ILP_ALWAYS_INLINE decltype(auto) propagate_return(Res& r, Ctrl& outer) {
            using C = std::remove_cvref_t<Ctrl>;
            if constexpr (std::is_same_v<C, EachCtrl>) {
                static_assert(always_false<Ctrl>,
                              "ILP_RETURN inside this nested loop cannot escape: the enclosing "
                              "ILP_FOR is closed with ILP_END. Change the enclosing loop's "
                              "ILP_END to ILP_END_RETURN so the value can propagate out.");
            } else if constexpr (std::is_same_v<C, ForCtrl>) {
                if constexpr (std::is_same_v<Res, ForResult>) {
                    outer.storage = std::move(r.storage);
                    outer.return_set = true;
                    outer.ok = false;
                } else {
                    outer.return_with(r.storage.extract()); // typed inner -> untyped outer
                }
            } else if constexpr (ctrl_typed_return<C>::is_typed) {
                using R2 = typename ctrl_typed_return<C>::type;
                if constexpr (std::is_same_v<Res, ForResult>) {
                    outer.return_with(r.storage.template extract<R2>());
                } else if constexpr (std::is_same_v<Res, ForResultTyped<R2>>) {
                    outer.storage.move_from(r.storage);
                    outer.return_set = true;
                    outer.ok = false;
                } else {
                    outer.return_with(r.storage.extract()); // typed inner -> typed outer
                }
            } else {
                // Top level: `outer` must be the global ilp_detail_ctrl() sentinel
                // function. Anything else here means either a new ctrl type was added
                // without a propagate_return branch, or a user-declared identifier
                // named ilp_detail_ctrl shadowed the sentinel - both would otherwise
                // silently swallow the value (the pre-fix bug class), so fail loudly.
                static_assert(std::is_function_v<C>,
                              "ILP_END_RETURN reached an unrecognized ctrl type. This is either an "
                              "ilp_for internal error (a ctrl type missing a propagate_return "
                              "branch) or a collision with a user-declared 'ilp_detail_ctrl' "
                              "identifier shadowing the ilp_for sentinel.");
                return *r; // Proxy converts to the enclosing function's return type
            }
        }

    } // namespace detail

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
            static_assert(std::in_range<T>(N),
                          "Unroll factor N must be representable by the loop index type.");
            static_assert(std::is_void_v<std::invoke_result_t<F&, T, Ctrl&>>,
                          "ilp_for loop bodies must return void; use ctrl.return_with(...) for "
                          "loop results and do not nest ILP_FOR macros inside function-API callbacks.");
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

        template<typename F, typename T>
        concept ForUntypedCtrlBody = std::invocable<F, T, ForCtrl&>;

        template<typename F, typename T, typename R>
        concept ForTypedCtrlBody = std::invocable<F, T, ForCtrlTyped<R>&>;

        template<std::size_t N, Mode M, std::integral T, typename F>
            requires ForUntypedCtrlBody<F, T>
        ForResult for_loop_untyped_impl(T start, T end, F&& body) {
            ForCtrl ctrl;
            index_loop_core<N, M>(start, end, ctrl, std::forward<F>(body));
            // Only move the storage when a value was actually stored; otherwise the
            // buffer holds indeterminate bytes that must not be copied.
            return ctrl.return_set ? ForResult{true, std::move(ctrl.storage)} : ForResult{false, {}};
        }

        template<typename R, std::size_t N, Mode M, std::integral T, typename F>
            requires ForTypedCtrlBody<F, T, R>
        ForResultTyped<R> for_loop_typed_impl(T start, T end, F&& body) {
            ForCtrlTyped<R> ctrl;
            index_loop_core<N, M>(start, end, ctrl, std::forward<F>(body));
            return ctrl.return_set ? ForResultTyped<R>{true, std::move(ctrl.storage)}
                                   : ForResultTyped<R>{false, {}};
        }

        // Macro entries always use default_mode: ILP_MODE_SIMPLE only flips
        // ilp::default_mode to Mode::Simple (see mode.hpp) - the macro layer is
        // unconditional and these entry points run in both build modes - so no
        // per-call Mode plumbing is needed here.
        //
        // R = void selects the untyped (SBO) return path; a non-void R selects the
        // typed path (ILP_FOR_T family). This file only exercises R = void; the
        // typed overload below is declared (loop_with_return_typed.cpp exercises
        // it) because macro_for's body names for_loop_typed_impl unconditionally -
        // see godbolt_examples/INSTRUCTIONS.md.
        template<std::size_t N, typename R = void, std::integral T, typename F>
        auto macro_for(T start, T end, F&& body, end_return_tag_t) {
            if constexpr (std::is_void_v<R>)
                return for_loop_untyped_impl<N, default_mode>(start, end, std::forward<F>(body));
            else
                return for_loop_typed_impl<R, N, default_mode>(start, end, std::forward<F>(body));
        }

    } // namespace detail

} // namespace ilp

// Fallback target for unqualified `ilp_detail_ctrl` lookup when ILP_END_RETURN is
// expanded at plain function scope - i.e. NOT nested inside another ILP_FOR body,
// where the body lambda's `ilp_detail_ctrl` parameter would otherwise shadow this.
// Declared as a function (not an object) so that shadowing it doesn't trigger
// -Wshadow/-Wshadow-all, which only cover variables/parameters/types, not ordinary
// functions.
inline void ilp_detail_ctrl() {}

#define ILP_FOR(loop_var_decl, start, end, N)                                                                          \
    if ([[maybe_unused]] auto ilp_detail_ret = [&]() { \
        return ::ilp::detail::macro_for<N>(start, end, \
            [&]([[maybe_unused]] loop_var_decl, [[maybe_unused]] auto& ilp_detail_ctrl)

#define ILP_END_RETURN , ::ilp::detail::end_return_tag_t{});                                                          \
    }                                                                                                                  \
    (); ilp_detail_ret) \
    return ::ilp::detail::propagate_return(ilp_detail_ret, ilp_detail_ctrl);                                          \
    else(void) 0

#define ILP_RETURN(x)                                                                                                  \
    do {                                                                                                               \
        ilp_detail_ctrl.return_with(x);                                                                               \
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
