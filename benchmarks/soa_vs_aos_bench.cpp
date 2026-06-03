// Microbenchmark backing the engine's core layout decision: integrating N
// bodies stored as structure-of-arrays vs the array-of-structs alternative.
// Run it before changing the storage strategy in snowglobe/ecs.

#include "snowglobe/ecs/soa_bodies.hpp"

#include <benchmark/benchmark.h>
#include <cstddef>
#include <vector>

namespace {

constexpr float kDt = 1.0F / 60.0F;

struct AosBody {
    float pos_x, pos_y;
    float vel_x, vel_y;
};

void BM_IntegrateAoS(benchmark::State& state) {
    const auto n = static_cast<std::size_t>(state.range(0));
    std::vector<AosBody> bodies(n, AosBody{0.0F, 0.0F, 1.0F, -1.0F});

    for (auto _ : state) {
        for (auto& b : bodies) {
            b.pos_x += b.vel_x * kDt;
            b.pos_y += b.vel_y * kDt;
        }
        benchmark::DoNotOptimize(bodies.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}

void BM_IntegrateSoA(benchmark::State& state) {
    const auto n = static_cast<std::size_t>(state.range(0));
    snowglobe::ecs::SoaBodies bodies;
    for (std::size_t i = 0; i < n; ++i) {
        bodies.add(0.0F, 0.0F, 1.0F, -1.0F);
    }

    for (auto _ : state) {
        bodies.integrate(kDt);
        benchmark::DoNotOptimize(bodies.pos_x.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}

} // namespace

BENCHMARK(BM_IntegrateAoS)->Range(1 << 10, 1 << 18);
BENCHMARK(BM_IntegrateSoA)->Range(1 << 10, 1 << 18);
