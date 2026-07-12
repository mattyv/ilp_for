// Benchmark for ilp::find_if - the dedicated vectorizable first-match search
// primitive (ilp_for/detail/find.hpp). Mirrors the ForBreakFixture conventions
// in bench_reduce.cpp: NOINLINE-extracted loops, a fixture seeding data with a
// break-at-midpoint match, and the same 10M-uint32 / threshold=500 scenario
// used by the Zen 5 probe that validated the blockcheck shape (see
// docs/PERFORMANCE.md).
#include "ilp_for.hpp"
#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

static constexpr uint32_t BENCH_SEED = 42;

// ==================== FIND_IF BENCHMARKS ====================

NOINLINE static size_t find_simple(const uint32_t* data, size_t size, uint32_t threshold) {
    for (size_t i = 0; i < size; ++i) {
        if (data[i] > threshold)
            return i;
    }
    return size;
}

NOINLINE static size_t find_std(const uint32_t* data, size_t size, uint32_t threshold) {
    auto it = std::find_if(data, data + size, [threshold](uint32_t v) { return v > threshold; });
    return static_cast<size_t>(it - data);
}

NOINLINE static size_t find_ilp_for(const uint32_t* data, size_t size, uint32_t threshold) {
    size_t result = size;
    ILP_FOR(auto i, size_t{0}, size, 4) {
        if (data[i] > threshold) {
            result = i;
            ILP_BREAK;
        }
    }
    ILP_END;
    return result;
}

NOINLINE static size_t find_blockcheck(const uint32_t* data, size_t size, uint32_t threshold) {
    auto span = std::span<const uint32_t>(data, size);
    auto it = ilp::find_if(span, [threshold](uint32_t v) { return v > threshold; });
    return static_cast<size_t>(it - span.begin());
}

template<std::size_t N>
NOINLINE static size_t find_blockcheck_n(const uint32_t* data, size_t size, uint32_t threshold) {
    auto span = std::span<const uint32_t>(data, size);
    auto it = ilp::find_if<N>(span, [threshold](uint32_t v) { return v > threshold; });
    return static_cast<size_t>(it - span.begin());
}

class FindIfFixture : public benchmark::Fixture {
  public:
    std::vector<uint32_t> data;
    uint32_t threshold;
    size_t break_pos;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        std::mt19937 rng(BENCH_SEED + 4);
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

BENCHMARK_DEFINE_F(FindIfFixture, Simple)(benchmark::State& state) {
    for (auto _ : state) {
        auto pos = find_simple(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(pos);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_DEFINE_F(FindIfFixture, Std)(benchmark::State& state) {
    for (auto _ : state) {
        auto pos = find_std(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(pos);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_DEFINE_F(FindIfFixture, IlpFor)(benchmark::State& state) {
    for (auto _ : state) {
        auto pos = find_ilp_for(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(pos);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_DEFINE_F(FindIfFixture, Blockcheck)(benchmark::State& state) {
    for (auto _ : state) {
        auto pos = find_blockcheck(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(pos);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_DEFINE_F(FindIfFixture, BlockcheckN16)(benchmark::State& state) {
    for (auto _ : state) {
        auto pos = find_blockcheck_n<16>(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(pos);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_DEFINE_F(FindIfFixture, BlockcheckN32)(benchmark::State& state) {
    for (auto _ : state) {
        auto pos = find_blockcheck_n<32>(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(pos);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

// Register all variants at 10M elements only - matching the Zen 5 probe that
// validated the blockcheck shape (docs/PERFORMANCE.md).
static constexpr size_t FIND_IF_BENCH_SIZE = 10'000'000;

BENCHMARK_REGISTER_F(FindIfFixture, Simple)->Arg(FIND_IF_BENCH_SIZE)->Unit(benchmark::kMillisecond);
BENCHMARK_REGISTER_F(FindIfFixture, Std)->Arg(FIND_IF_BENCH_SIZE)->Unit(benchmark::kMillisecond);
BENCHMARK_REGISTER_F(FindIfFixture, IlpFor)->Arg(FIND_IF_BENCH_SIZE)->Unit(benchmark::kMillisecond);
BENCHMARK_REGISTER_F(FindIfFixture, Blockcheck)->Arg(FIND_IF_BENCH_SIZE)->Unit(benchmark::kMillisecond);
BENCHMARK_REGISTER_F(FindIfFixture, BlockcheckN16)->Arg(FIND_IF_BENCH_SIZE)->Unit(benchmark::kMillisecond);
BENCHMARK_REGISTER_F(FindIfFixture, BlockcheckN32)->Arg(FIND_IF_BENCH_SIZE)->Unit(benchmark::kMillisecond);

// ==================== FIND_IF uint8_t BENCHMARKS ====================
//
// Pins the ~30x Clang win at the smallest measured element size (see
// docs/PERFORMANCE.md): a byte-scaled default block size is what lets
// find_if scale UP its advantage as elements get smaller, unlike a
// flat/element-count-only default would. Same break-at-midpoint pattern as
// FindIfFixture above, scaled to uint8_t's range: values 0-99, match value
// 201 (comfortably above the 0-99 fill and below uint8_t's 255 max),
// threshold 200.

NOINLINE static size_t find_simple_u8(const uint8_t* data, size_t size, uint8_t threshold) {
    for (size_t i = 0; i < size; ++i) {
        if (data[i] > threshold)
            return i;
    }
    return size;
}

NOINLINE static size_t find_std_u8(const uint8_t* data, size_t size, uint8_t threshold) {
    auto it = std::find_if(data, data + size, [threshold](uint8_t v) { return v > threshold; });
    return static_cast<size_t>(it - data);
}

NOINLINE static size_t find_blockcheck_u8(const uint8_t* data, size_t size, uint8_t threshold) {
    auto span = std::span<const uint8_t>(data, size);
    auto it = ilp::find_if(span, [threshold](uint8_t v) { return v > threshold; });
    return static_cast<size_t>(it - span.begin());
}

class FindIfU8Fixture : public benchmark::Fixture {
  public:
    std::vector<uint8_t> data;
    uint8_t threshold;
    size_t break_pos;

    void SetUp(const benchmark::State& state) override {
        size_t size = state.range(0);
        data.resize(size);
        std::mt19937 rng(BENCH_SEED + 5);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(rng() % 100);
        }
        break_pos = size / 2;
        data[break_pos] = 201;
        threshold = 200;
    }

    void TearDown(const benchmark::State&) override {
        data.clear();
        data.shrink_to_fit();
    }
};

BENCHMARK_DEFINE_F(FindIfU8Fixture, Simple)(benchmark::State& state) {
    for (auto _ : state) {
        auto pos = find_simple_u8(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(pos);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_DEFINE_F(FindIfU8Fixture, Std)(benchmark::State& state) {
    for (auto _ : state) {
        auto pos = find_std_u8(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(pos);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_DEFINE_F(FindIfU8Fixture, Blockcheck)(benchmark::State& state) {
    for (auto _ : state) {
        auto pos = find_blockcheck_u8(data.data(), data.size(), threshold);
        benchmark::DoNotOptimize(pos);
    }
    state.SetItemsProcessed(state.iterations() * break_pos);
}

BENCHMARK_REGISTER_F(FindIfU8Fixture, Simple)->Arg(FIND_IF_BENCH_SIZE)->Unit(benchmark::kMillisecond);
BENCHMARK_REGISTER_F(FindIfU8Fixture, Std)->Arg(FIND_IF_BENCH_SIZE)->Unit(benchmark::kMillisecond);
BENCHMARK_REGISTER_F(FindIfU8Fixture, Blockcheck)->Arg(FIND_IF_BENCH_SIZE)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
