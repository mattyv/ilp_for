// Reproduces the measurements behind find_if's default block sizes. Run on
// new hardware (e.g. Apple Silicon) before re-tuning the constants in
// ilp_for/detail/find.hpp.
//
// Sweep: is the optimal blockcheck N an element count or a byte count? Types
// u8..u64, N in {8,16,32,64,128,256}, break-at-midpoint, best-of-25, plus a
// plain scalar loop baseline for comparison. This is a standalone tool (not a
// Google Benchmark target) so it can be built and run with a single compiler
// invocation on hardware where fetching/building the benchmark dependency is
// inconvenient.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

template<typename T, std::size_t N>
__attribute__((noinline)) std::size_t blockcheck(const T* data, std::size_t size, T threshold) {
    std::size_t i = 0;
    const std::size_t block_end = size - size % N;
    for (; i != block_end; i += N) {
        bool any = false;
        for (std::size_t j = 0; j < N; ++j)
            any |= data[i + j] > threshold;
        if (any)
            break;
    }
    for (; i < size; ++i)
        if (data[i] > threshold)
            return i;
    return size;
}

template<typename T>
__attribute__((noinline)) std::size_t simple(const T* data, std::size_t size, T threshold) {
    for (std::size_t i = 0; i < size; ++i)
        if (data[i] > threshold)
            return i;
    return size;
}

// Number of timed repetitions per variant; the reported result is the best
// (minimum) across these, matching the methodology used for every other
// measurement cited in docs/PERFORMANCE.md.
static constexpr int SWEEP_REPETITIONS = 25;

template<typename F, typename T>
double bench(F f, const T* d, std::size_t n, T t, std::size_t expect) {
    double best = 1e18;
    for (int r = 0; r < SWEEP_REPETITIONS; ++r) {
        auto t0 = std::chrono::steady_clock::now();
        std::size_t got = f(d, n, t);
        auto t1 = std::chrono::steady_clock::now();
        if (got != expect) {
            std::printf("WRONG %zu != %zu\n", got, expect);
            return -1;
        }
        best = std::min(best, std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    return best;
}

template<typename T>
void run(const char* name) {
    const std::size_t n = 10'000'000;
    std::vector<T> data(n);
    std::mt19937 rng(42);
    for (auto& v : data)
        v = static_cast<T>(rng() % 100);
    // 200 (threshold) and 201 (the injected match value below) both fit
    // every instantiated type here, including uint8_t (max 255) - no
    // per-type fallback needed.
    const T threshold = T{200};
    data[n / 2] = static_cast<T>(threshold + 1); // 201
    const std::size_t expect = n / 2;
    const T* d = data.data();

    std::printf("%-4s (%zuB): simple %7.3f", name, sizeof(T), bench(simple<T>, d, n, threshold, expect));
    std::printf("  N8 %7.3f", bench(blockcheck<T, 8>, d, n, threshold, expect));
    std::printf("  N16 %7.3f", bench(blockcheck<T, 16>, d, n, threshold, expect));
    std::printf("  N32 %7.3f", bench(blockcheck<T, 32>, d, n, threshold, expect));
    std::printf("  N64 %7.3f", bench(blockcheck<T, 64>, d, n, threshold, expect));
    std::printf("  N128 %7.3f", bench(blockcheck<T, 128>, d, n, threshold, expect));
    std::printf("  N256 %7.3f\n", bench(blockcheck<T, 256>, d, n, threshold, expect));
}

int main() {
    run<uint8_t>("u8");
    run<uint16_t>("u16");
    run<uint32_t>("u32");
    run<uint64_t>("u64");
    return 0;
}
