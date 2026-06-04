// Toolchain smoke benchmark: confirms Google Benchmark links and runs. Replace
// with real hot-path microbenchmarks (e.g. SoA vs AoS) as the engine is designed.

#include <benchmark/benchmark.h>

namespace {

void BM_Noop(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(state.iterations());
    }
}

} // namespace

BENCHMARK(BM_Noop);
