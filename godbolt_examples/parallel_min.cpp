// parallel min - godbolt example

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace ilp {
    namespace detail {

        template<typename T>
        struct is_optional : std::false_type {};
        template<typename T>
        struct is_optional<std::optional<T>> : std::true_type {};
        template<typename T>
        inline constexpr bool is_optional_v = is_optional<T>::value;

        template<typename Op, typename T>
        constexpr bool has_known_identity_v =
            std::is_same_v<std::decay_t<Op>, std::plus<>> || std::is_same_v<std::decay_t<Op>, std::plus<T>> ||
            std::is_same_v<std::decay_t<Op>, std::multiplies<>> ||
            std::is_same_v<std::decay_t<Op>, std::multiplies<T>> || std::is_same_v<std::decay_t<Op>, std::bit_and<>> ||
            std::is_same_v<std::decay_t<Op>, std::bit_and<T>> || std::is_same_v<std::decay_t<Op>, std::bit_or<>> ||
            std::is_same_v<std::decay_t<Op>, std::bit_or<T>> || std::is_same_v<std::decay_t<Op>, std::bit_xor<>> ||
            std::is_same_v<std::decay_t<Op>, std::bit_xor<T>>;

        template<typename Op, typename T>
        constexpr T make_identity() {
            if constexpr (std::is_same_v<std::decay_t<Op>, std::plus<>> ||
                          std::is_same_v<std::decay_t<Op>, std::plus<T>>) {
                return T{};
            } else if constexpr (std::is_same_v<std::decay_t<Op>, std::multiplies<>> ||
                                 std::is_same_v<std::decay_t<Op>, std::multiplies<T>>) {
                return T{1};
            } else if constexpr (std::is_same_v<std::decay_t<Op>, std::bit_and<>> ||
                                 std::is_same_v<std::decay_t<Op>, std::bit_and<T>>) {
                return ~T{};
            } else if constexpr (std::is_same_v<std::decay_t<Op>, std::bit_or<>> ||
                                 std::is_same_v<std::decay_t<Op>, std::bit_or<T>>) {
                return T{};
            } else if constexpr (std::is_same_v<std::decay_t<Op>, std::bit_xor<>> ||
                                 std::is_same_v<std::decay_t<Op>, std::bit_xor<T>>) {
                return T{};
            } else {
                static_assert(has_known_identity_v<Op, T>, "make_identity requires known operation");
            }
        }

        template<std::size_t N, typename BinaryOp, typename R, typename Init>
        std::array<R, N> make_accumulators([[maybe_unused]] const BinaryOp& op, [[maybe_unused]] Init&& init) {
            if constexpr (has_known_identity_v<BinaryOp, R>) {
                return []<std::size_t... Is>(std::index_sequence<Is...>) {
                    return std::array<R, N>{((void)Is, make_identity<BinaryOp, R>())...};
                }(std::make_index_sequence<N>{});
            } else {
                std::array<R, N> accs;
                accs[0] = static_cast<R>(std::forward<Init>(init));
                for (std::size_t i = 1; i < N; ++i) {
                    accs[i] = accs[0];
                }
                return accs;
            }
        }

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
        concept ReduceBody = std::invocable<F, T> && !std::same_as<std::invoke_result_t<F, T>, void>;

        template<std::size_t N, std::integral T, typename Init, typename BinaryOp, typename F>
            requires ReduceBody<F, T>
        auto reduce_impl(T start, T end, Init&& init, BinaryOp op, F&& body) {
            validate_unroll_factor<N>();

            using ResultT = std::invoke_result_t<F, T>;

            if constexpr (is_optional_v<ResultT>) {
                using R = typename ResultT::value_type;
                auto accs = make_accumulators<N, BinaryOp, R>(op, std::forward<Init>(init));

                T i = start;
                bool should_break = false;

                for (; i + static_cast<T>(N) <= end && !should_break; i += static_cast<T>(N)) {
                    for (std::size_t j = 0; j < N && !should_break; ++j) {
                        auto result = body(i + static_cast<T>(j));
                        if (!result) {
                            should_break = true;
                        } else {
                            accs[j] = op(accs[j], *result);
                        }
                    }
                }

                for (; i < end && !should_break; ++i) {
                    auto result = body(i);
                    if (!result) {
                        should_break = true;
                    } else {
                        accs[0] = op(accs[0], *result);
                    }
                }

                return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    R out = std::forward<Init>(init);
                    ((out = op(out, accs[Is])), ...);
                    return out;
                }(std::make_index_sequence<N>{});
            } else {
                using R = ResultT;
                auto accs = make_accumulators<N, BinaryOp, R>(op, std::forward<Init>(init));

                T i = start;

                for (; i + static_cast<T>(N) <= end; i += static_cast<T>(N)) {
                    for (std::size_t j = 0; j < N; ++j) {
                        accs[j] = op(accs[j], body(i + static_cast<T>(j)));
                    }
                }

                for (; i < end; ++i) {
                    accs[0] = op(accs[0], body(i));
                }

                return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                    R out = std::forward<Init>(init);
                    ((out = op(out, accs[Is])), ...);
                    return out;
                }(std::make_index_sequence<N>{});
            }
        }

    } // namespace detail

    template<std::size_t N = 4, std::integral T, typename Init, typename BinaryOp, typename F>
        requires detail::ReduceBody<F, T>
    auto reduce(T start, T end, Init&& init, BinaryOp op, F&& body) {
        return detail::reduce_impl<N>(start, end, std::forward<Init>(init), op, std::forward<F>(body));
    }

} // namespace ilp

// ilp version
int find_min_ilp(const std::vector<int>& data) {
    auto min_op = [](int a, int b) { return std::min(a, b); };
    return ilp::reduce<4>(0uz, data.size(), std::numeric_limits<int>::max(), min_op, [&](auto i) { return data[i]; });
}

// hand-rolled
int find_min_handrolled(const std::vector<int>& data) {
    int min0 = std::numeric_limits<int>::max();
    int min1 = std::numeric_limits<int>::max();
    int min2 = std::numeric_limits<int>::max();
    int min3 = std::numeric_limits<int>::max();
    size_t i = 0;

    for (; i + 4 <= data.size(); i += 4) {
        min0 = std::min(min0, data[i]);
        min1 = std::min(min1, data[i + 1]);
        min2 = std::min(min2, data[i + 2]);
        min3 = std::min(min3, data[i + 3]);
    }

    for (; i < data.size(); ++i) {
        min0 = std::min(min0, data[i]);
    }

    return std::min({min0, min1, min2, min3});
}

// simple
int find_min_simple(const std::vector<int>& data) {
    int min_val = std::numeric_limits<int>::max();
    for (size_t i = 0; i < data.size(); ++i) {
        min_val = std::min(min_val, data[i]);
    }
    return min_val;
}

int main() {
    volatile size_t n = 1000;
    std::vector<int> data(n);

    for (size_t i = 0; i < n; ++i) {
        data[i] = (i * 7) % 100;
    }

    int min1 = find_min_ilp(data);
    int min2 = find_min_handrolled(data);
    int min3 = find_min_simple(data);

    return (min1 == min2 && min2 == min3) ? 0 : 1;
}
