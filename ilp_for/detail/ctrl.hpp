// ilp_for - ILP loop unrolling for C++23
// Copyright (c) 2025 Matt Vanderdorff
// https://github.com/mattyv/ilp_for
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

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

    // Small storage for integral types (8 bytes max - covers int, size_t, pointers)
    // Only supports trivially destructible types to avoid lifetime management complexity.
    // Use ILP_FOR_T for non-trivial return types.
    struct SmallStorage {
        alignas(8) char buffer[8];

        template<typename T>
        ILP_ALWAYS_INLINE void set(T&& val) {
            using U = std::decay_t<T>;
            static_assert(sizeof(U) <= 8, "Return type exceeds 8 bytes. Use ILP_FOR_T(type, ...) instead.");
            static_assert(alignof(U) <= 8, "Return type alignment exceeds 8. Use ILP_FOR_T(type, ...) instead.");
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
            new (buffer) R(static_cast<T&&>(val));
        }

        ILP_ALWAYS_INLINE R extract() {
            R* ptr = std::launder(reinterpret_cast<R*>(buffer));
            R tmp = static_cast<R&&>(*ptr);
            ptr->~R();
            return tmp;
        }
    };

    // ok=false means early exit (simple version - 8-byte storage)
    struct ForCtrl {
        bool ok = true;
        bool return_set = false;
        SmallStorage storage;
    };

    // ok=false means early exit (typed version - exact size storage)
    template<typename R>
    struct ForCtrlTyped {
        bool ok = true;
        bool return_set = false;
        TypedStorage<R> storage;
    };

    struct [[nodiscard("ILP_RETURN value ignored - did you mean ILP_END_RETURN?")]] ForResult {
        bool has_return;
        SmallStorage storage;

        explicit operator bool() const noexcept { return has_return; }

        // deduces type from function return
        struct Proxy {
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

        [[noreturn]] inline void ilp_end_with_return_error() {
            std::fprintf(stderr, "\n*** ILP_FOR ERROR ***\n"
                                 "ILP_RETURN was called but ILP_END was used instead of ILP_END_RETURN.\n"
                                 "The return value would be silently discarded. This is a bug.\n"
                                 "Fix: Change ILP_END to ILP_END_RETURN in the enclosing function.\n\n");
            std::abort();
        }

    } // namespace detail

} // namespace ilp
