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

    // 64 bytes fits most types
    struct AnyStorage {
        alignas(std::max_align_t) char buffer[64];

        template<typename T>
        [[gnu::always_inline]] inline void set(T&& val) {
            using U = std::decay_t<T>;
            static_assert(sizeof(U) <= sizeof(buffer), "Return type too large for ILP_RETURN (max 64 bytes)");
            static_assert(alignof(U) <= alignof(std::max_align_t), "Return type alignment too strict");
            new (buffer) U(static_cast<T&&>(val));
        }

        template<typename R>
        [[gnu::always_inline]] inline R extract() {
            return static_cast<R&&>(*reinterpret_cast<R*>(buffer));
        }
    };

    // ok=false means early exit
    struct ForCtrl {
        bool ok = true;
        bool return_set = false;
        AnyStorage storage;
    };

    struct [[nodiscard("ILP_RETURN value ignored - did you mean ILP_END_RETURN?")]] ForResult {
        bool has_return;
        AnyStorage storage;

        explicit operator bool() const noexcept { return has_return; }

        // deduces type from function return
        struct Proxy {
            AnyStorage& s;

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

    } // namespace detail

} // namespace ilp
