// ilp_for - ILP loop unrolling for C++20
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstddef>
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

    template<typename R = void>
    struct LoopCtrl {
        bool ok = true;
        std::optional<R> return_value;

        void break_loop() { ok = false; }
        void return_with(R val) {
            ok = false;
            return_value = std::move(val);
        }
    };

    template<>
    struct LoopCtrl<void> {
        bool ok = true;
        void break_loop() { ok = false; }
    };

    // Small buffer optimization for integral types.
    // Buffer size matches the largest integral type on this architecture (typically 8 bytes).
    // Only supports trivially destructible types to avoid lifetime management complexity.
    // Use ILP_FOR_T for non-trivial or larger return types.
    struct SmallStorage {
        alignas(arch::sbo_size) char buffer[arch::sbo_size];

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
            static_assert(std::is_trivially_destructible_v<U>,
                          "SmallStorage only supports trivially-destructible types. "
                          "Use ILP_FOR_T(type, ...) for non-trivial return types.");
            new (buffer) U(static_cast<T&&>(val));
        }

        template<typename R>
        ILP_ALWAYS_INLINE R extract() {
            return static_cast<R&&>(*std::launder(reinterpret_cast<R*>(buffer)));
        }
    };

    // Typed storage for user-specified types (exact size)
    // Properly destructs stored object after extraction to avoid leaks.
    template<typename R>
    struct TypedStorage {
        alignas(R) char buffer[sizeof(R)];

        template<typename T>
        ILP_ALWAYS_INLINE void set(T&& val) {
            static_assert(!detail::is_proxy_v<std::decay_t<T>>,
                          "Cannot store a loop-result Proxy: extract the value into a typed local "
                          "first, or return it directly with `return *std::move(r);`.");
            new (buffer) R(static_cast<T&&>(val));
        }

        ILP_ALWAYS_INLINE R extract() {
            R* ptr = std::launder(reinterpret_cast<R*>(buffer));
            R tmp = static_cast<R&&>(*ptr);
            ptr->~R();
            return tmp;
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

    // ok=false means early exit (typed version - exact size storage)
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

#if defined(_MSC_VER) && !defined(__clang__)
            // MSVC needs explicit overloads without && qualifier
            // templated conversion operators don't deduce properly in return statements
            template<typename T>
            operator std::optional<T>() {
                return std::optional<T>(s.template extract<T>());
            }

            template<typename R, std::enable_if_t<!detail::is_optional_v<R>, int> = 0>
            operator R() {
                return s.template extract<R>();
            }
#else
            // GCC/Clang do implicit Proxy→T→optional
            template<typename R>
                requires(!detail::is_optional_v<R>)
            ILP_ALWAYS_INLINE operator R() && {
                return s.template extract<R>();
            }
#endif

            void operator*() && {}
        };

        ILP_ALWAYS_INLINE Proxy operator*() { return {storage}; }
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

#if defined(_MSC_VER) && !defined(__clang__)
            operator std::optional<R>() { return std::optional<R>(s.extract()); }

            operator R() { return s.extract(); }
#else
            ILP_ALWAYS_INLINE operator R() && { return s.extract(); }
#endif
        };

        ILP_ALWAYS_INLINE Proxy operator*() { return {storage}; }
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
                    outer.storage = r.storage; // type-erased byte copy, same pun contract
                    outer.return_set = true;
                    outer.ok = false;
                } else {
                    outer.return_with(r.storage.extract()); // typed inner -> untyped outer
                }
            } else if constexpr (ctrl_typed_return<C>::is_typed) {
                using R2 = typename ctrl_typed_return<C>::type;
                if constexpr (std::is_same_v<Res, ForResult>) {
                    outer.return_with(r.storage.template extract<R2>());
                } else {
                    outer.return_with(r.storage.extract()); // typed inner -> typed outer
                }
            } else {
                return *r; // top level: Proxy converts to the function's return type
            }
        }

    } // namespace detail

} // namespace ilp
