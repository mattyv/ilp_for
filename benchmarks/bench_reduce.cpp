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

// Simple for loop
BENCHMARK_DEFINE_F(SumFixture, Simple)(benchmark::State& state) {
    for (auto _ : state) {
        uint32_t sum = 0;
        for (auto val : data) sum += val;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

// std::accumulate
BENCHMARK_DEFINE_F(SumFixture, StdAccumulate)(benchmark::State& state) {
    for (auto _ : state) {
        uint32_t sum = std::accumulate(data.begin(), data.end(), 0u);
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

// Handrolled 4 accumulators
BENCHMARK_DEFINE_F(SumFixture, Handrolled)(benchmark::State& state) {
    for (auto _ : state) {
        uint32_t sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
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
        uint32_t sum = sum0 + sum1 + sum2 + sum3;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

// ILP library
BENCHMARK_DEFINE_F(SumFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        std::span<const uint32_t> arr(data);
        uint32_t sum = ILP_REDUCE_RANGE_SUM(val, arr, 4) {
            return val;
        } ILP_END_REDUCE;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

// Register benchmarks with different sizes
BENCHMARK_REGISTER_F(SumFixture, Simple)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_REGISTER_F(SumFixture, StdAccumulate)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_REGISTER_F(SumFixture, Handrolled)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_REGISTER_F(SumFixture, ILP)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

// ==================== SUM STEP 2 BENCHMARKS ====================
class SumStep2Fixture : public benchmark::Fixture {
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

BENCHMARK_DEFINE_F(SumStep2Fixture, Simple)(benchmark::State& state) {
    for (auto _ : state) {
        uint32_t sum = 0;
        for (size_t i = 0; i < data.size(); i += 2) sum += data[i];
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * data.size() / 2);
}

BENCHMARK_DEFINE_F(SumStep2Fixture, Handrolled)(benchmark::State& state) {
    for (auto _ : state) {
        uint32_t sum0 = 0, sum1 = 0, sum2 = 0, sum3 = 0;
        const uint32_t* ptr = data.data();
        size_t n = data.size();
        size_t i = 0;
        for (; i + 8 <= n; i += 8) {
            sum0 += ptr[i];
            sum1 += ptr[i + 2];
            sum2 += ptr[i + 4];
            sum3 += ptr[i + 6];
        }
        for (; i < n; i += 2) sum0 += ptr[i];
        uint32_t sum = sum0 + sum1 + sum2 + sum3;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * data.size() / 2);
}

BENCHMARK_DEFINE_F(SumStep2Fixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        uint32_t sum = ILP_REDUCE_STEP_SUM(i, 0u, (unsigned)data.size(), 2u, 4) {
            return data[i];
        } ILP_END_REDUCE;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * data.size() / 2);
}

BENCHMARK_REGISTER_F(SumStep2Fixture, Simple)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_REGISTER_F(SumStep2Fixture, Handrolled)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_REGISTER_F(SumStep2Fixture, ILP)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

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
        uint32_t sum = ILP_REDUCE(std::plus<>{}, 0u, i, 0u, (unsigned)data.size(), 4) {
            if (i >= stop_at) {
                ILP_BREAK_RET(0u);
            }
            return data[i];
        } ILP_END_REDUCE;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * stop_at);
}

BENCHMARK_REGISTER_F(SumBreakFixture, Simple)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_REGISTER_F(SumBreakFixture, ILP)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

// ==================== FIND BENCHMARKS ====================
class FindFixture : public benchmark::Fixture {
public:
    std::vector<uint32_t> data;
    uint32_t target;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = i % 100;
        }
        target = 99; // Last value in pattern - worst case
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

BENCHMARK_DEFINE_F(FindFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        auto result = ilp::for_loop_range_idx_ret_simple<std::size_t, 4>(
            std::span<const uint32_t>(data),
            [&](auto val, auto idx) -> std::optional<std::size_t> {
                if (val == target) return idx;
                return std::nullopt;
            });
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_REGISTER_F(FindFixture, StdFind)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_REGISTER_F(FindFixture, ILP)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

// ==================== COUNT BENCHMARKS ====================
class CountFixture : public benchmark::Fixture {
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

BENCHMARK_DEFINE_F(CountFixture, StdCountIf)(benchmark::State& state) {
    for (auto _ : state) {
        auto count = std::count_if(data.begin(), data.end(), [](uint32_t v) { return v > 50; });
        benchmark::DoNotOptimize(count);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_DEFINE_F(CountFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        std::span<const uint32_t> arr(data);
        auto count = ILP_REDUCE_RANGE_SUM(val, arr, 4) {
            return val > 50 ? 1u : 0u;
        } ILP_END_REDUCE;
        benchmark::DoNotOptimize(count);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_REGISTER_F(CountFixture, StdCountIf)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_REGISTER_F(CountFixture, ILP)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

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
        auto min_val = ILP_REDUCE_RANGE_SIMPLE([](auto a, auto b){ return a < b ? a : b; },
            std::numeric_limits<uint32_t>::max(), val, arr, 4) {
            return val;
        } ILP_END_REDUCE;
        benchmark::DoNotOptimize(min_val);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_REGISTER_F(MinFixture, StdMinElement)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_REGISTER_F(MinFixture, Handrolled)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_REGISTER_F(MinFixture, ILP)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

// ==================== ANY BENCHMARKS ====================
class AnyFixture : public benchmark::Fixture {
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

BENCHMARK_DEFINE_F(AnyFixture, StdAnyOf)(benchmark::State& state) {
    for (auto _ : state) {
        bool found = std::any_of(data.begin(), data.end(), [](uint32_t v) { return v == 99; });
        benchmark::DoNotOptimize(found);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_DEFINE_F(AnyFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        std::span<const uint32_t> arr(data);
        auto count = ILP_REDUCE_RANGE_SUM(val, arr, 4) {
            return val == 99 ? 1u : 0u;
        } ILP_END_REDUCE;
        bool found = count > 0;
        benchmark::DoNotOptimize(found);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_REGISTER_F(AnyFixture, StdAnyOf)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_REGISTER_F(AnyFixture, ILP)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

// ==================== ALL BENCHMARKS ====================
class AllFixture : public benchmark::Fixture {
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

BENCHMARK_DEFINE_F(AllFixture, StdAllOf)(benchmark::State& state) {
    for (auto _ : state) {
        bool all = std::all_of(data.begin(), data.end(), [](uint32_t v) { return v < 100; });
        benchmark::DoNotOptimize(all);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_DEFINE_F(AllFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        std::span<const uint32_t> arr(data);
        auto count = ILP_REDUCE_RANGE_SUM(val, arr, 4) {
            return val < 100 ? 1u : 0u;
        } ILP_END_REDUCE;
        bool all = count == data.size();
        benchmark::DoNotOptimize(all);
    }
    state.SetItemsProcessed(state.iterations() * data.size());
}

BENCHMARK_REGISTER_F(AllFixture, StdAllOf)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_REGISTER_F(AllFixture, ILP)
    ->Arg(1000)->Arg(10000)->Arg(100000)->Arg(1000000)->Arg(10000000)
    ->Unit(benchmark::kMillisecond);
