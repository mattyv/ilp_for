#pragma once

#include <concepts>

namespace ilp {

template<std::integral T>
struct iota_view {
    T first, last;

    struct iterator {
        T value;
        constexpr T operator*() const { return value; }
        constexpr iterator& operator++() { ++value; return *this; }
        constexpr bool operator!=(iterator o) const { return value != o.value; }
    };

    constexpr iterator begin() const { return {first}; }
    constexpr iterator end() const { return {last}; }
};

template<std::integral T>
constexpr iota_view<T> iota(T start, T end) { return {start, end}; }

} // namespace ilp
