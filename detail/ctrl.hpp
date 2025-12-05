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
    // Trait to detect std::optional - needed to avoid ambiguous conversion
    template<typename T> struct is_optional : std::false_type {};
    template<typename T> struct is_optional<std::optional<T>> : std::true_type {};
    template<typename T> inline constexpr bool is_optional_v = is_optional<T>::value;
}

// =============================================================================
// Typed control (legacy - used by ILP_FOR(ret_type, ...) overloads)
// =============================================================================

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

// =============================================================================
// Type-erased control (new API - ILP_FOR(var, ...) without return type)
// =============================================================================

// Type-erased storage for return values (64 bytes covers most types)
struct AnyStorage {
    alignas(std::max_align_t) char buffer[64];

    template<typename T>
    [[gnu::always_inline]] inline
    void set(T&& val) {
        using U = std::decay_t<T>;
        static_assert(sizeof(U) <= sizeof(buffer), "Return type too large for ILP_RETURN (max 64 bytes)");
        static_assert(alignof(U) <= alignof(std::max_align_t), "Return type alignment too strict");
        new (buffer) U(static_cast<T&&>(val));
    }

    template<typename R>
    [[gnu::always_inline]] inline
    R extract() {
        return static_cast<R&&>(*reinterpret_cast<R*>(buffer));
    }
};

// Control structure with inline storage
// - ok: false means early exit requested (ILP_BREAK or ILP_RETURN)
// - return_set: true means ILP_RETURN was called and storage contains a value
// This distinction allows ILP_BREAK to work in void functions without returning
struct ForCtrl {
    bool ok = true;
    bool return_set = false;
    AnyStorage storage;
};

// Result wrapper returned from untyped for_loop
// operator bool() returns true if ILP_RETURN was called
// operator*() returns a proxy that converts to the target type
struct [[nodiscard("ILP_RETURN value ignored - did you mean ILP_END_RETURN?")]] ForResult {
    bool has_return;
    AnyStorage storage;

    explicit operator bool() const noexcept { return has_return; }

    // Proxy enables "return *_ilp_ret_" to deduce type from function return
    struct Proxy {
        AnyStorage& s;

        // Exclude std::optional to avoid ambiguity with optional's converting constructor
        // When returning optional<T>, the value stored is T, not optional<T>
        template<typename R>
            requires (!detail::is_optional_v<R>)
        [[gnu::always_inline]] inline
        operator R() && {
            return s.template extract<R>();
        }

        // For void functions - makes "return *_ilp_ret_" compile
        void operator*() && {}
    };

    [[gnu::always_inline]] inline
    Proxy operator*() { return {storage}; }
};

namespace detail {

// Called when ILP_RETURN is used but ILP_END (not ILP_END_RETURN) was specified
// This is a programming error - always aborts with a clear message
[[noreturn]] inline void ilp_end_with_return_error() {
    // Use fprintf to stderr for maximum compatibility (no exceptions, works in signal handlers)
    std::fprintf(stderr,
        "\n*** ILP_FOR ERROR ***\n"
        "ILP_RETURN was called but ILP_END was used instead of ILP_END_RETURN.\n"
        "The return value would be silently discarded. This is a bug.\n"
        "Fix: Change ILP_END to ILP_END_RETURN in the enclosing function.\n\n");
    std::abort();
}

} // namespace detail

} // namespace ilp
