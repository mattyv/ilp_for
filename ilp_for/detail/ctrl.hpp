// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

#include "arch.hpp"

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
// ILP_NO_DEBUG_TYPECHECK (e.g. to keep debug-build layout identical to release
// when mixing TUs - see the README's Large Return Types section for the ODR
// caveat this implies). Zero members / zero code when disabled.
#if !defined(ILP_NO_DEBUG_TYPECHECK) && (defined(ILP_DEBUG_TYPECHECK) || !defined(NDEBUG))
#define ILP_TYPECHECK_ENABLED 1
#include <cstdio>
#include <cstdlib>
#else
#define ILP_TYPECHECK_ENABLED 0
#endif

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
        // about to go out of scope, so storing it (e.g. via a hand-written
        // `ctrl.return_with(*std::move(r))`) would store a dangling reference.
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
        // (An EMPTY result's Proxy may be discarded freely - the check is gated on
        // ilp_debug_has_value, so e.g. `*std::move(r);` on a no-match result is a
        // silent no-op, same as before this check existed.)
        [[noreturn]] inline void swallowed_proxy_abort() {
            std::fprintf(stderr,
                         "\n*** ilp_for swallowed return value ***\n"
                         "A loop result holding a value was destroyed without being converted.\n"
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
            // Signature text can collide for distinct anonymous-namespace types in different TUs.
            if (ilp_debug_stored_tag != nullptr &&
                std::strcmp(ilp_debug_stored_tag->name, detail::type_tag_v<Rt>.name) != 0)
                detail::type_mismatch_abort(ilp_debug_stored_tag->name, detail::type_tag_v<Rt>.name);
#endif
            std::array<std::byte, sizeof(Rt)> representation{};
            std::memcpy(representation.data(), buffer, sizeof(Rt));
            Rt result = std::bit_cast<Rt>(representation);
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

} // namespace ilp
