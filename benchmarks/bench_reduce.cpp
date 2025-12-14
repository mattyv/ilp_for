#include "ilp_for.hpp"
#include <algorithm>
#include <benchmark/benchmark.h>
#include <limits>
#include <numeric>
#include <random>
#include <span>
#include <vector>

// Fixed seed for reproducible benchmarks
static constexpr uint32_t BENCH_SEED = 42;

// Fixture for sum benchmarks
class SumFixture : public benchmark::Fixture {
  public:
    std::vector<uint32_t> data;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = i % 100;
        }
    }

    void TearDown(const benchmark::State&) override {
        data.clear();
        data.shrink_to_fit();
    }
};

// std::accumulate baseline
BENCHMARK_DEFINE_F(SumFixture, StdAccumulate)(benchmark::State& state) {
    for (auto _ : state) {
        uint64_t sum = std::accumulate(data.begin(), data.end(), 0ull);
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

// Handrolled 4 accumulators
BENCHMARK_DEFINE_F(SumFixture, Handrolled)(benchmark::State& state) {
    for (auto _ : state) {
        uint64_t sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
        const uint32_t* ptr = data.data();
        size_t n = data.size();
        size_t i = 0;
        for (; i + 4 <= n; i += 4) {
            sum0 += ptr[i];
            sum1 += ptr[i + 1];
            sum2 += ptr[i + 2];
            sum3 += ptr[i + 3];
        }
        for (; i < n; ++i)
            sum0 += ptr[i];
        uint64_t sum = sum0 + sum1 + sum2 + sum3;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

// ILP library (function API)
BENCHMARK_DEFINE_F(SumFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        std::span<const uint32_t> arr(data);
        uint64_t sum = ilp::transform_reduce_auto<ilp::LoopType::Sum>(
            arr, 0ull, std::plus<>{}, [](auto&& val) { return static_cast<uint64_t>(val); });
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

// Register benchmarks with different sizes
BENCHMARK_REGISTER_F(SumFixture, StdAccumulate)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(SumFixture, Handrolled)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(SumFixture, ILP)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

// ==================== SUM WITH BREAK BENCHMARKS ====================
class SumBreakFixture : public benchmark::Fixture {
  public:
    std::vector<uint32_t> data;
    unsigned stop_at;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = i % 100;
        }
        stop_at = size / 2; // Break halfway
    }

    void TearDown(const benchmark::State&) override {
        data.clear();
        data.shrink_to_fit();
    }
};

BENCHMARK_DEFINE_F(SumBreakFixture, Simple)(benchmark::State& state) {
    for (auto _ : state) {
        uint32_t sum = 0;
        for (unsigned i = 0; i < data.size(); ++i) {
            if (i >= stop_at)
                break;
            sum += data[i];
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * stop_at);
}

BENCHMARK_DEFINE_F(SumBreakFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        uint64_t sum =
            ilp::transform_reduce<4>(std::views::iota(0u, (unsigned)data.size()), 0ull, std::plus<>{}, [&](auto i) -> std::optional<uint64_t> {
                if (i >= stop_at)
                    return std::nullopt;
                return static_cast<uint64_t>(data[i]);
            });
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * stop_at);
}

BENCHMARK_REGISTER_F(SumBreakFixture, Simple)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(SumBreakFixture, ILP)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

// ==================== FIND BENCHMARKS ====================
class FindFixture : public benchmark::Fixture {
  public:
    std::vector<uint32_t> data;
    uint32_t target;
    size_t expected_pos; // For accurate items processed count

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint32_t>(i);
        }
        // Shuffle data for realistic branch prediction behavior
        std::mt19937 rng(BENCH_SEED);
        std::shuffle(data.begin(), data.end(), rng);
        // Pick target at ~50% position on average
        expected_pos = size / 2;
        target = data[expected_pos];
    }

    void TearDown(const benchmark::State&) override {
        data.clear();
        data.shrink_to_fit();
    }
};

BENCHMARK_DEFINE_F(FindFixture, StdFind)(benchmark::State& state) {
    for (auto _ : state) {
        auto it = std::find(data.begin(), data.end(), target);
        benchmark::DoNotOptimize(it);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

// Optimized find - matches std::find performance
BENCHMARK_DEFINE_F(FindFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        std::span<const uint32_t> arr(data);
        auto it = ilp::find_if_auto(arr, [&](auto&& val) { return val == target; });
        benchmark::DoNotOptimize(it);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_REGISTER_F(FindFixture, StdFind)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(FindFixture, ILP)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

// ==================== MIN BENCHMARKS ====================
class MinFixture : public benchmark::Fixture {
  public:
    std::vector<uint32_t> data;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = (i * 7 + 13) % 10000; // Pseudo-random
        }
    }

    void TearDown(const benchmark::State&) override {
        data.clear();
        data.shrink_to_fit();
    }
};

BENCHMARK_DEFINE_F(MinFixture, StdMinElement)(benchmark::State& state) {
    for (auto _ : state) {
        auto it = std::min_element(data.begin(), data.end());
        benchmark::DoNotOptimize(*it);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

// Hand-rolled with 4 accumulators (fair comparison to ILP)
BENCHMARK_DEFINE_F(MinFixture, Handrolled)(benchmark::State& state) {
    for (auto _ : state) {
        uint32_t min0 = UINT32_MAX, min1 = UINT32_MAX, min2 = UINT32_MAX, min3 = UINT32_MAX;
        const uint32_t* ptr = data.data();
        size_t n = data.size();
        size_t i = 0;
        for (; i + 4 <= n; i += 4) {
            if (ptr[i] < min0)
                min0 = ptr[i];
            if (ptr[i + 1] < min1)
                min1 = ptr[i + 1];
            if (ptr[i + 2] < min2)
                min2 = ptr[i + 2];
            if (ptr[i + 3] < min3)
                min3 = ptr[i + 3];
        }
        for (; i < n; ++i) {
            if (ptr[i] < min0)
                min0 = ptr[i];
        }
        uint32_t result = std::min({min0, min1, min2, min3});
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_DEFINE_F(MinFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        std::span<const uint32_t> arr(data);
        auto min_val = ilp::transform_reduce_auto<ilp::LoopType::MinMax>(
            arr, std::numeric_limits<uint32_t>::max(), [](auto a, auto b) { return a < b ? a : b; },
            [](auto&& val) { return val; });
        benchmark::DoNotOptimize(min_val);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_REGISTER_F(MinFixture, StdMinElement)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(MinFixture, Handrolled)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(MinFixture, ILP)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

// ==================== ANY BENCHMARKS ====================
class AnyFixture : public benchmark::Fixture {
  public:
    std::vector<uint32_t> data;
    uint32_t target;
    size_t expected_pos;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint32_t>(i);
        }
        // Shuffle data for realistic branch prediction behavior
        std::mt19937 rng(BENCH_SEED + 1); // Different seed than Find
        std::shuffle(data.begin(), data.end(), rng);
        // Pick target at ~50% position on average
        expected_pos = size / 2;
        target = data[expected_pos];
    }

    void TearDown(const benchmark::State&) override {
        data.clear();
        data.shrink_to_fit();
    }
};

BENCHMARK_DEFINE_F(AnyFixture, StdAnyOf)(benchmark::State& state) {
    for (auto _ : state) {
        bool found = std::any_of(data.begin(), data.end(), [&](uint32_t v) { return v == target; });
        benchmark::DoNotOptimize(found);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

// Optimized find for any_of pattern
BENCHMARK_DEFINE_F(AnyFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        std::span<const uint32_t> arr(data);
        auto it = ilp::find_if_auto(arr, [&](auto&& val) { return val == target; });
        bool found = (it != arr.end());
        benchmark::DoNotOptimize(found);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_REGISTER_F(AnyFixture, StdAnyOf)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(AnyFixture, ILP)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

// ==================== ILP_FOR WITH ILP_BREAK BENCHMARKS ====================
// Tests early exit from a FOR loop (not reduce) - e.g., validate until first failure
class ForBreakFixture : public benchmark::Fixture {
  public:
    std::vector<uint32_t> data;
    uint32_t threshold;
    size_t break_pos;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        // Fill with values 0-99, place a "bad" value (1000) at ~50% position
        std::mt19937 rng(BENCH_SEED + 2);
        for (size_t i = 0; i < size; ++i) {
            data[i] = rng() % 100;
        }
        break_pos = size / 2;
        data[break_pos] = 1000; // Value that triggers break
        threshold = 500;
    }

    void TearDown(const benchmark::State&) override {
        data.clear();
        data.shrink_to_fit();
    }
};

BENCHMARK_DEFINE_F(ForBreakFixture, Simple)(benchmark::State& state) {
    for (auto _ : state) {
        size_t count = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            if (data[i] > threshold)
                break;
            ++count;
        }
        benchmark::DoNotOptimize(count);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_DEFINE_F(ForBreakFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        size_t count = 0;
        ILP_FOR(auto i, 0uz, data.size(), 4) {
            if (data[i] > threshold)
                ILP_BREAK;
            ++count;
        }
        ILP_END;
        benchmark::DoNotOptimize(count);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_DEFINE_F(ForBreakFixture, PragmaUnroll)(benchmark::State& state) {
    for (auto _ : state) {
        size_t count = 0;
#if defined(__clang__)
#pragma clang loop unroll_count(4)
#elif defined(__GNUC__)
#pragma GCC unroll 4
#endif
        for (size_t i = 0; i < data.size(); ++i) {
            if (data[i] > threshold)
                break;
            ++count;
        }
        benchmark::DoNotOptimize(count);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_REGISTER_F(ForBreakFixture, Simple)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(ForBreakFixture, ILP)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(ForBreakFixture, PragmaUnroll)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

// ==================== ILP_FOR WITH RETURN TYPE BENCHMARKS ====================
// Tests returning a computed value using ILP_FOR with non-void return type
class ForRetFixture : public benchmark::Fixture {
  public:
    std::vector<uint32_t> data;
    uint32_t target;
    size_t target_pos;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint32_t>(i);
        }
        std::mt19937 rng(BENCH_SEED + 3);
        std::shuffle(data.begin(), data.end(), rng);
        target_pos = size / 2;
        target = data[target_pos];
    }

    void TearDown(const benchmark::State&) override {
        data.clear();
        data.shrink_to_fit();
    }
};

// Helper functions using ILP_FOR with return type
static std::optional<uint64_t> find_and_compute_simple(const std::vector<uint32_t>& data, uint32_t target) {
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == target) {
            return static_cast<uint64_t>(i) * data[i]; // Return computed value
        }
    }
    return std::nullopt;
}

static std::optional<uint64_t> find_and_compute_ilp(const std::vector<uint32_t>& data, uint32_t target) {
    ILP_FOR(auto i, 0uz, data.size(), 4) {
        if (data[i] == target) {
            ILP_RETURN(static_cast<uint64_t>(i) * data[i]);
        }
    }
    ILP_END_RETURN;
    return std::nullopt;
}

BENCHMARK_DEFINE_F(ForRetFixture, Simple)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = find_and_compute_simple(data, target);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * target_pos);
}

BENCHMARK_DEFINE_F(ForRetFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = find_and_compute_ilp(data, target);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * target_pos);
}

BENCHMARK_REGISTER_F(ForRetFixture, Simple)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(ForRetFixture, ILP)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

// ==================== FIND WITH INDEX BENCHMARKS ====================
// Tests finding with both value and index access
class FindIdxFixture : public benchmark::Fixture {
  public:
    std::vector<uint32_t> data;
    uint32_t target;
    size_t target_pos;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint32_t>(i);
        }
        std::mt19937 rng(BENCH_SEED + 4);
        std::shuffle(data.begin(), data.end(), rng);
        target_pos = size / 2;
        target = data[target_pos];
    }

    void TearDown(const benchmark::State&) override {
        data.clear();
        data.shrink_to_fit();
    }
};

BENCHMARK_DEFINE_F(FindIdxFixture, Simple)(benchmark::State& state) {
    for (auto _ : state) {
        auto it = data.end();
        for (auto curr = data.begin(); curr != data.end(); ++curr) {
            if (*curr == target) {
                it = curr;
                break;
            }
        }
        benchmark::DoNotOptimize(it);
    }
    state.SetItemsProcessed(state.iterations() * target_pos);
}

BENCHMARK_DEFINE_F(FindIdxFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        std::span<const uint32_t> arr(data);
        auto it = ilp::find_range_idx_auto(arr, [&](auto&& val, auto, auto) { return val == target; });
        benchmark::DoNotOptimize(it);
    }
    state.SetItemsProcessed(state.iterations() * target_pos);
}

BENCHMARK_REGISTER_F(FindIdxFixture, Simple)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(FindIdxFixture, ILP)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
