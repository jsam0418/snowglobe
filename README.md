# Snowglobe

A server-authoritative **2D physics engine** in **C++23**. A headless backend
runs the simulation on a Kubernetes cluster and streams world state to browser
clients over **WebSockets**; rendering happens client-side. The planned engine
uses an Entity Component System with structure-of-arrays storage and a wait-free
SPSC queue between the network I/O thread and the simulation thread.

**Status:** project scaffold — toolchain, container, CI/CD, and GitOps deploy
are in place and verified; the engine itself is being designed.

Live: [game.josiahsam.com](https://game.josiahsam.com)

## Quick start

Everything builds and runs inside a single container that doubles as the dev,
CI, and production environment (Clang 18, CMake, Ninja, vcpkg + deps,
clang-tidy/format, GoogleTest, Google Benchmark):

```bash
docker build -t snowglobe:dev .
docker run --rm -it -v "$PWD":/workspace snowglobe:dev bash

# inside the container:
cmake --preset release
cmake --build build/release
ctest --preset debug-tsan        # ThreadSanitizer build
./build/release/snowglobe        # WebSocket server on :9001 (SNOWGLOBE_PORT to override)
```

## Layout

| Path | What |
| --- | --- |
| `src/main.cpp` | uWebSockets server skeleton (bare echo for now) |
| `tests/` | GoogleTest (toolchain smoke test) |
| `benchmarks/` | Google Benchmark (toolchain smoke benchmark) |
| `deploy/` | Kubernetes manifests (Kustomize overlay) |
| `clusters/production/` | Flux GitOps objects |
| `docs/FRAMEWORKS.md` | Toolchain & environment spec |

The engine source (ECS, simulation core, SPSC hand-off) is not written yet — it
lives under `src/` as it's designed.

## Build presets

`release`, `debug-asan` (ASan + UBSan), `debug-tsan` (ThreadSanitizer — required
by the spec for the I/O ↔ sim hand-off). See `CMakePresets.json`.

Contributor / agent notes live in [CLAUDE.md](CLAUDE.md).
