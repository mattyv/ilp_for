#include "ilp_for.hpp"
#include <algorithm>
#include <benchmark/benchmark.h>
#include <optional>
#include <random>
#include <vector>

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

static constexpr uint32_t BENCH_SEED = 42;

// ==================== ILP_FOR WITH ILP_BREAK BENCHMARKS ====================

// Extract loops to noinline functions to prevent benchmark harness from
// interfering with loop optimization
NOINLINE static size_t break_simple(const uint32_t* data, size_t size, uint32_t threshold) {
    size_t count = 0;
    for (size_t i = 0; i < size; ++i) {
        if (data[i] > threshold)
            break;
        ++count;
    }
    return count;
}

NOINLINE static size_t break_ilp(const uint32_t* data, size_t size, uint32_t threshold) {
    size_t count = 0;
    ILP_FOR(auto i, size_t{0}, size, 4) {
        if (data[i] > threshold)
            ILP_BREAK;
        ++count;
    }
    ILP_END;
    return count;
}

NOINLINE static size_t break_pragma(const uint32_t* data, size_t size, uint32_t threshold) {
    size_t count = 0;
#if defined(__clang__)
#pragma clang loop unroll_count(4)
#elif defined(__GNUC__)
#pragma GCC unroll 4
#endif
    for (size_t i = 0; i < size; ++i) {
        if (data[i] > threshold)
            break;
        ++count;
    }
    return count;
}

class ForBreakFixture : public benchmark::Fixture {
  public:
    std::vector<uint32_t> data;
    uint32_t threshold;
    size_t break_pos;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        std::mt19937 rng(BENCH_SEED + 2);
        for (size_t i = 0; i < size; ++i) {
            data[i] = rng() % 100;
        }
        break_pos = size / 2;
        data[break_pos] = 1000;
        threshold = 500;
    }

    void TearDown(const benchmark::State&) override {
        data.clear();
        data.shrink_to_fit();
    }
};

BENCHMARK_DEFINE_F(ForBreakFixture, Simple)(benchmark::State& state) {
    for (auto _ : state) {
        auto count = break_simple(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(count);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_DEFINE_F(ForBreakFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        auto count = break_ilp(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(count);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_DEFINE_F(ForBreakFixture, PragmaUnroll)(benchmark::State& state) {
    for (auto _ : state) {
        auto count = break_pragma(data.data(), data.size(), threshold);
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

static std::optional<uint64_t> find_and_compute_simple(const std::vector<uint32_t>& data, uint32_t target) {
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == target) {
            return static_cast<uint64_t>(i) * data[i];
        }
    }
    return std::nullopt;
}

static std::optional<uint64_t> find_and_compute_ilp(const std::vector<uint32_t>& data, uint32_t target) {
    ILP_FOR(auto i, size_t{0}, data.size(), 4) {
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

BENCHMARK_DEFINE_F(ForRetFixture, PragmaUnroll)(benchmark::State& state) {
    for (auto _ : state) {
        std::optional<uint64_t> result;
#if defined(__clang__)
#pragma clang loop unroll_count(4)
#elif defined(__GNUC__)
#pragma GCC unroll 4
#endif
        for (size_t i = 0; i < data.size(); ++i) {
            if (data[i] == target) {
                result = static_cast<uint64_t>(i) * data[i];
                break;
            }
        }
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

BENCHMARK_REGISTER_F(ForRetFixture, PragmaUnroll)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

// ==================== ILP_FOR WITH ILP_CONTINUE BENCHMARKS ====================
// Pattern: filtered search - skip even numbers, find first value > threshold
// The comparisons are independent, so ILP helps

NOINLINE static size_t continue_simple(const uint32_t* data, size_t size, uint32_t threshold) {
    for (size_t i = 0; i < size; ++i) {
        if (data[i] % 2 == 0)
            continue; // Skip even numbers
        if (data[i] > threshold)
            return i; // Found it
    }
    return size;
}

NOINLINE static size_t continue_ilp(const uint32_t* data, size_t size, uint32_t threshold) {
    ILP_FOR(auto i, size_t{0}, size, 4) {
        if (data[i] % 2 == 0)
            ILP_CONTINUE;
        if (data[i] > threshold)
            ILP_RETURN(i);
    }
    ILP_END_RETURN;
    return size;
}

NOINLINE static size_t continue_pragma(const uint32_t* data, size_t size, uint32_t threshold) {
#if defined(__clang__)
#pragma clang loop unroll_count(4)
#elif defined(__GNUC__)
#pragma GCC unroll 4
#endif
    for (size_t i = 0; i < size; ++i) {
        if (data[i] % 2 == 0)
            continue;
        if (data[i] > threshold)
            return i;
    }
    return size;
}

class ForContinueFixture : public benchmark::Fixture {
  public:
    std::vector<uint32_t> data;
    uint32_t threshold;
    size_t match_pos;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        std::mt19937 rng(BENCH_SEED + 4);
        for (size_t i = 0; i < size; ++i) {
            data[i] = rng() % 100; // Values 0-99, roughly 50% even
        }
        // Place an odd value > threshold at midpoint
        match_pos = size / 2;
        data[match_pos] = 501; // Odd and > 500
        threshold = 500;
    }

    void TearDown(const benchmark::State&) override {
        data.clear();
        data.shrink_to_fit();
    }
};

BENCHMARK_DEFINE_F(ForContinueFixture, Simple)(benchmark::State& state) {
    for (auto _ : state) {
        auto idx = continue_simple(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(idx);
    }
    state.SetItemsProcessed(state.iterations() * match_pos);
}

BENCHMARK_DEFINE_F(ForContinueFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        auto idx = continue_ilp(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(idx);
    }
    state.SetItemsProcessed(state.iterations() * match_pos);
}

BENCHMARK_DEFINE_F(ForContinueFixture, PragmaUnroll)(benchmark::State& state) {
    for (auto _ : state) {
        auto idx = continue_pragma(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(idx);
    }
    state.SetItemsProcessed(state.iterations() * match_pos);
}

BENCHMARK_REGISTER_F(ForContinueFixture, Simple)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(ForContinueFixture, ILP)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(ForContinueFixture, PragmaUnroll)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

// ==================== ILP_FOR_RANGE BENCHMARKS ====================

NOINLINE static size_t range_simple(const std::vector<uint32_t>& data, uint32_t threshold) {
    size_t count = 0;
    for (auto val : data) {
        if (val > threshold)
            break;
        ++count;
    }
    return count;
}

NOINLINE static size_t range_ilp(const std::vector<uint32_t>& data, uint32_t threshold) {
    size_t count = 0;
    ILP_FOR_RANGE(auto val, data, 4) {
        if (val > threshold)
            ILP_BREAK;
        ++count;
    }
    ILP_END;
    return count;
}

class ForRangeFixture : public benchmark::Fixture {
  public:
    std::vector<uint32_t> data;
    uint32_t threshold;
    size_t break_pos;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        std::mt19937 rng(BENCH_SEED + 5);
        for (size_t i = 0; i < size; ++i) {
            data[i] = rng() % 100;
        }
        break_pos = size / 2;
        data[break_pos] = 1000;
        threshold = 500;
    }

    void TearDown(const benchmark::State&) override {
        data.clear();
        data.shrink_to_fit();
    }
};

BENCHMARK_DEFINE_F(ForRangeFixture, Simple)(benchmark::State& state) {
    for (auto _ : state) {
        auto count = range_simple(data, threshold);
        benchmark::DoNotOptimize(count);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_DEFINE_F(ForRangeFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        auto count = range_ilp(data, threshold);
        benchmark::DoNotOptimize(count);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_REGISTER_F(ForRangeFixture, Simple)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(ForRangeFixture, ILP)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

// ==================== ILP_FOR_AUTO BENCHMARKS ====================

NOINLINE static size_t auto_simple(const uint32_t* data, size_t size, uint32_t threshold) {
    size_t count = 0;
    for (size_t i = 0; i < size; ++i) {
        if (data[i] > threshold)
            break;
        ++count;
    }
    return count;
}

NOINLINE static size_t auto_ilp(const uint32_t* data, size_t size, uint32_t threshold) {
    size_t count = 0;
    ILP_FOR_AUTO(auto i, size_t{0}, size, Search, double) {
        if (data[i] > threshold)
            ILP_BREAK;
        ++count;
    }
    ILP_END;
    return count;
}

class ForAutoFixture : public benchmark::Fixture {
  public:
    std::vector<uint32_t> data;
    uint32_t threshold;
    size_t break_pos;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        std::mt19937 rng(BENCH_SEED + 6);
        for (size_t i = 0; i < size; ++i) {
            data[i] = rng() % 100;
        }
        break_pos = size / 2;
        data[break_pos] = 1000;
        threshold = 500;
    }

    void TearDown(const benchmark::State&) override {
        data.clear();
        data.shrink_to_fit();
    }
};

BENCHMARK_DEFINE_F(ForAutoFixture, Simple)(benchmark::State& state) {
    for (auto _ : state) {
        auto count = auto_simple(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(count);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_DEFINE_F(ForAutoFixture, ILP)(benchmark::State& state) {
    for (auto _ : state) {
        auto count = auto_ilp(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(count);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_REGISTER_F(ForAutoFixture, Simple)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_REGISTER_F(ForAutoFixture, ILP)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000)
    ->Arg(1000000)
    ->Arg(10000000)
    ->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
