#include <benchmark/benchmark.h>
#include <vector>
#include <numeric>
#include <algorithm>
#include <limits>
#include <span>
#include "ilp_for.hpp"

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
        for (; i < n; ++i) sum0 += ptr[i];
        uint64_t sum = sum0 + sum1 + sum2 + sum3;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

// ILP library
BENCHMARK_DEFINE_F(SumFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        std::span<const uint32_t> arr(data);
        uint64_t sum = ILP_REDUCE_RANGE_SIMPLE_AUTO(std::plus<>{}, 0ull, auto&& val, arr) {
            return static_cast<uint64_t>(val);
        } ILP_END_REDUCE;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

// Register benchmarks with different sizes
#if !defined(ILP_MODE_PRAGMA)
BENCHMARK_REGISTER_F(SumFixture, Handrolled)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);
#endif

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
            if (i >= stop_at) break;
            sum += data[i];
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * stop_at);
}

BENCHMARK_DEFINE_F(SumBreakFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        uint64_t sum = ILP_REDUCE(std::plus<>{}, 0ull, auto i, 0u, (unsigned)data.size(), 4) {
            if (i >= stop_at) {
                ILP_BREAK_RET(0ull);
            }
            return static_cast<uint64_t>(data[i]);
        } ILP_END_REDUCE;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * stop_at);
}

BENCHMARK_REGISTER_F(SumBreakFixture, Simple)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(SumBreakFixture, ILP)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

// ==================== FIND BENCHMARKS ====================
class FindFixture : public benchmark::Fixture {
public:
    std::vector<uint32_t> data;
    uint32_t target;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = i;  // Unique values
        }
        target = size - 1; // Last element - worst case
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

// Optimized for_until - matches std::find performance
BENCHMARK_DEFINE_F(FindFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        std::span<const uint32_t> arr(data);
        auto idx = ILP_FOR_UNTIL_RANGE(auto&& val, arr, 8) {
            return val == target;
        } ILP_END_UNTIL;
        benchmark::DoNotOptimize(idx);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_REGISTER_F(FindFixture, StdFind)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(FindFixture, ILP)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
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
            if (ptr[i] < min0) min0 = ptr[i];
            if (ptr[i + 1] < min1) min1 = ptr[i + 1];
            if (ptr[i + 2] < min2) min2 = ptr[i + 2];
            if (ptr[i + 3] < min3) min3 = ptr[i + 3];
        }
        for (; i < n; ++i) {
            if (ptr[i] < min0) min0 = ptr[i];
        }
        uint32_t result = std::min({min0, min1, min2, min3});
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_DEFINE_F(MinFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        std::span<const uint32_t> arr(data);
        auto min_val = ILP_REDUCE_RANGE_SIMPLE_AUTO([](auto a, auto b){ return a < b ? a : b; },
            std::numeric_limits<uint32_t>::max(), auto&& val, arr) {
            return val;
        } ILP_END_REDUCE;
        benchmark::DoNotOptimize(min_val);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_REGISTER_F(MinFixture, StdMinElement)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(MinFixture, Handrolled)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(MinFixture, ILP)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

// ==================== ANY BENCHMARKS ====================
class AnyFixture : public benchmark::Fixture {
public:
    std::vector<uint32_t> data;
    uint32_t target;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = i;  // Unique values
        }
        target = size - 1;  // Last element - worst case
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

// Optimized for_until for any_of pattern
BENCHMARK_DEFINE_F(AnyFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        std::span<const uint32_t> arr(data);
        auto idx = ILP_FOR_UNTIL_RANGE(auto&& val, arr, 8) {
            return val == target;
        } ILP_END_UNTIL;
        bool found = idx.has_value();
        benchmark::DoNotOptimize(found);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_REGISTER_F(AnyFixture, StdAnyOf)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(AnyFixture, ILP)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kNanosecond);
