#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace snowglobe::ecs {

using Entity = std::uint32_t;

// Structure-of-Arrays storage for rigid-body state.
//
// The engine deliberately favours SoA over the more intuitive
// array-of-structs (Body{pos, vel, ...}[]) layout: the hot integration loop
// touches positions and velocities contiguously, so packing each field in its
// own array keeps the cache lines dense and lets the vectorizer go to work.
// The `soa_vs_aos` microbenchmark exists to keep this assumption honest.
struct SoaBodies {
    std::vector<float> pos_x;
    std::vector<float> pos_y;
    std::vector<float> vel_x;
    std::vector<float> vel_y;

    Entity add(float px, float py, float vx, float vy) {
        const auto id = static_cast<Entity>(pos_x.size());
        pos_x.push_back(px);
        pos_y.push_back(py);
        vel_x.push_back(vx);
        vel_y.push_back(vy);
        return id;
    }

    [[nodiscard]] std::size_t size() const noexcept { return pos_x.size(); }

    // Semi-implicit Euler step over every body.
    void integrate(float dt) {
        const std::size_t n = pos_x.size();
        for (std::size_t i = 0; i < n; ++i) {
            pos_x[i] += vel_x[i] * dt;
            pos_y[i] += vel_y[i] * dt;
        }
    }
};

} // namespace snowglobe::ecs
