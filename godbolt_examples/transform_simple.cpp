// simple transform - godbolt example (SIMPLE mode)

#include <concepts>
#include <cstddef>
#include <vector>

namespace ilp {

    template<std::integral T>
    struct iota_view {
        T first, last;

        struct iterator {
            T value;
            constexpr T operator*() const { return value; }
            constexpr iterator& operator++() {
                ++value;
                return *this;
            }
            constexpr bool operator!=(iterator o) const { return value != o.value; }
        };

        constexpr iterator begin() const { return {first}; }
        constexpr iterator end() const { return {last}; }
    };

    template<std::integral T>
    constexpr iota_view<T> iota(T start, T end) {
        return {start, end};
    }

} // namespace ilp

#define ILP_FOR(loop_var_decl, start, end, N) for (loop_var_decl : ::ilp::iota((start), (end)))
#define ILP_END

// ilp version
void transform_ilp(std::vector<int>& data) {
    ILP_FOR(auto i, 0uz, data.size(), 4) {
        data[i] = data[i] * 2 + 1;
    }
    ILP_END;
}

// hand-rolled
void transform_handrolled(std::vector<int>& data) {
    size_t i = 0;
    for (; i + 4 <= data.size(); i += 4) {
        data[i] = data[i] * 2 + 1;
        data[i + 1] = data[i + 1] * 2 + 1;
        data[i + 2] = data[i + 2] * 2 + 1;
        data[i + 3] = data[i + 3] * 2 + 1;
    }

    for (; i < data.size(); ++i) {
        data[i] = data[i] * 2 + 1;
    }
}

// simple
void transform_simple(std::vector<int>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = data[i] * 2 + 1;
    }
}

int main() {
    volatile size_t n = 1000;

    std::vector<int> data1(n);
    std::vector<int> data2(n);
    std::vector<int> data3(n);

    for (size_t i = 0; i < n; ++i) {
        data1[i] = data2[i] = data3[i] = i;
    }

    transform_ilp(data1);
    transform_handrolled(data2);
    transform_simple(data3);

    return (data1[0] == data2[0] && data2[0] == data3[0]) ? 0 : 1;
}
