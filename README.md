# Snowglobe

A server-authoritative **2D physics engine** in **C++23**. A headless backend
runs the simulation on a Kubernetes cluster and streams world state to browser
clients over **WebSockets**; rendering happens client-side. State lives in a
cache-friendly Entity Component System (structure-of-arrays), and a wait-free
SPSC queue hands work between the network I/O thread and the simulation thread.

Live: [game.josiahsam.com](https://game.josiahsam.com)

## Quick start

Everything builds and runs inside a single container that doubles as the dev,
CI, and production environment (Clang 18, CMake, Ninja, vcpkg + deps,
clang-tidy/format, GoogleTest, Google Benchmark):

```bash
docker build -t snowglobe:dev .
docker run --rm -it -v "$PWD":/workspace snowglobe:dev

# inside the container:
cmake --preset release
cmake --build build/release
ctest --preset debug-tsan        # validates the lock-free SPSC queue
./build/release/snowglobe        # WebSocket server on :9001 (SNOWGLOBE_PORT to override)
```

## Layout

| Path | What |
| --- | --- |
| `src/main.cpp` | uWebSockets server / event loop (I/O thread) |
| `src/snowglobe/core/` | Simulation core, incl. the SPSC queue |
| `src/snowglobe/ecs/` | Structure-of-arrays body storage |
| `tests/` | GoogleTest unit tests |
| `benchmarks/` | Google Benchmark microbenchmarks (e.g. SoA vs AoS) |
| `docs/FRAMEWORKS.md` | Toolchain & environment spec |

## Build presets

`release`, `debug-asan` (ASan + UBSan), `debug-tsan` (ThreadSanitizer — required
for the SPSC queue). See `CMakePresets.json`.

Contributor / agent notes live in [CLAUDE.md](CLAUDE.md).
