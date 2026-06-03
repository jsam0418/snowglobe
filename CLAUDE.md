# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Snowglobe is a **server-authoritative 2D physics engine** written in **C++23**. A
headless backend runs the simulation and streams world state to browser clients
over **WebSockets**; rendering is entirely client-side. It is deployed as a
container on Kubernetes (public URL `game.josiahsam.com`).

`docs/FRAMEWORKS.md` is the authoritative environment/tooling spec — consult it
before changing toolchain, build, or dependency setup. It is intentionally
tooling-only and does **not** describe application design.

## Everything runs in one container

There is a **single Docker image used for development, CI, and production**
(zero toolchain drift — see `docs/FRAMEWORKS.md` "Containerization"). It carries
the full toolchain: Clang 18, CMake, Ninja, vcpkg with the dependency set
prebuilt into a binary cache, clang-tidy/clang-format, GoogleTest, Google
Benchmark. Do work inside it:

```bash
docker build -t snowglobe:dev .                            # pin deps: --build-arg VCPKG_REF=<tag>
docker run --rm -it -v "$PWD":/workspace snowglobe:dev bash # dev shell at /workspace
```

`VCPKG_ROOT` and the vcpkg binary cache are baked in, so the CMake configure
step below restores prebuilt `uWebSockets`/`gtest`/`benchmark` instead of
recompiling them.

The Dockerfile also **bakes a release build of the server** into the image
(`COPY . .` then `cmake --build`), so the same image is the deployable
artifact and its default `CMD` runs `./build/release/snowglobe`. Source changes
only invalidate from that `COPY` down — the dependency layers stay cached. For
dev you override the command (`… snowglobe:dev bash`) and bind-mount the working
tree, which shadows the baked build so you iterate against your checkout.

## Build, test, benchmark

Builds are driven by **CMake presets** (`CMakePresets.json`). Each preset builds
into `build/<preset>/`. There are three:

- `release` — optimized.
- `debug-asan` — Debug + AddressSanitizer + UndefinedBehaviorSanitizer.
- `debug-tsan` — Debug + ThreadSanitizer. **Required** validation for the
  lock-free SPSC queue (`src/snowglobe/core/spsc_queue.hpp`); run it after
  touching anything in the I/O-thread ↔ sim-thread hand-off.

```bash
# Configure + build (swap the preset name as needed)
cmake --preset release
cmake --build build/release

# Run the unit tests (CTest)
ctest --preset debug-asan
ctest --preset debug-tsan          # the one that matters for the SPSC queue

# Run a single test by name (GoogleTest filter)
./build/debug-asan/tests/snowglobe_tests --gtest_filter='SpscQueue.*'

# Microbenchmarks (build with the `release` preset, then run the binary)
./build/release/benchmarks/snowglobe_benchmarks

# Run the server (defaults to port 9001; override with SNOWGLOBE_PORT)
./build/release/snowglobe
```

## Lint / format

```bash
clang-format -i $(git ls-files '*.cpp' '*.hpp')      # config: .clang-format
clang-tidy -p build/release src/main.cpp             # config: .clang-tidy
```

clang-tidy and clangd are fed by the exported `compile_commands.json`
(`CMAKE_EXPORT_COMPILE_COMMANDS=ON`); configure at least once so it exists.
`.clang-tidy`'s `HeaderFilterRegex` deliberately scopes analysis to
`src/`, `tests/`, `benchmarks/` so dependency headers are never linted.

## Architecture and conventions that span files

- **Threading model.** A uWebSockets event loop owns the network I/O thread
  (`src/main.cpp`). The simulation runs on a separate thread. They communicate
  through a single **wait-free SPSC ring buffer** (`spsc_queue.hpp`) — one
  producer, one consumer, acquire/release ordering. This is the project's most
  delicate code; it is why `debug-tsan` exists and is non-negotiable.
- **Data layout is Structure-of-Arrays, not Array-of-Structs.** Body state lives
  in parallel arrays (`src/snowglobe/ecs/soa_bodies.hpp`) so the hot integration
  loop stays cache-dense and vectorizable. The `soa_vs_aos` benchmark guards this
  decision — re-run it before changing storage layout, and don't reintroduce
  AoS bodies without a benchmark to justify it.
- **`-Werror` is scoped to our targets only.** Project warning flags, `-Werror`,
  and sanitizers live on the `snowglobe_project_options` INTERFACE library and
  are attached only via `target_link_libraries(... PRIVATE ...)` on our own
  targets. Dependencies never link it, and their include dirs are SYSTEM by
  default, so **a warning in a uWS/vcpkg header must never break the build** —
  preserve this when adding targets (link `snowglobe_project_options`, don't set
  warning flags globally).
- **Dependencies go through vcpkg manifest mode** (`vcpkg.json`) — the single
  dependency manager. Add a library by editing `vcpkg.json`, then `find_package`
  it in CMake; do not vendor or `FetchContent`. Rebuild the image so the new dep
  enters the binary cache.

## CI/CD and deployment (GitHub-only)

> This supersedes the Gitea / Gitea-Actions / mirror sections of
> `docs/FRAMEWORKS.md`: the project is GitHub-only. Flux is repo-agnostic, so
> "deploy from Gitea" simply became "deploy from GitHub."

- **CI** (`.github/workflows/ci.yml`) builds the image once, loads it, and runs
  every pipeline stage *inside that exact image* — clang-format check →
  clang-tidy (`--warnings-as-errors="*"`) → release tests → ASan/UBSan → TSan.
  Testing the artifact we ship is the point; don't move stages onto the bare
  runner. GHA layer caching keeps the dependency layers warm.
- **Publish** (on `main` only): the image is built multi-arch
  (`linux/amd64,linux/arm64`) and pushed to **GHCR** at
  `ghcr.io/<owner>/snowglobe:<git-sha>` via the built-in `GITHUB_TOKEN`. Tags are
  immutable SHAs — never `:latest`.
- **Deploy is GitOps via Flux.** Manifests live in `deploy/` (a Kustomize
  overlay). On `main`, CI rewrites the image name+tag in
  `deploy/kustomization.yaml` and commits it back with `[skip ci]` (this is
  "Strategy A" — CI commits the tag). Flux (`clusters/production/snowglobe.yaml`:
  a `GitRepository` + `Kustomization`) reconciles `deploy/` and the cluster pulls
  the image from GHCR. Flux never touches image bytes.
- **`replicas: 1` is deliberate** — the sim is server-authoritative with
  in-memory world state; you can't scale out without sharding worlds first.
- **Arch caveat:** GitHub runners are amd64, so the arm64 image is built under
  QEMU emulation (slow). If the cluster is arm64 (e.g. a Pi cluster), registering
  a self-hosted arm64 runner builds natively and faster — still GitHub, no Gitea.

## Hard constraints (from docs/FRAMEWORKS.md)

- C++23, Clang 18+. Use any C++23 feature **except modules / `import std;`** (v1
  ban — still rough in Clang/libc++).
- Standard library: **libstdc++** (the image installs g++-14/libstdc++-14 and
  builds everything, including vcpkg deps, with Clang against it). Keep it
  consistent everywhere; do not mix in libc++.
- Out of scope for v1 unless a concrete pain appears: IWYU, scan-build, ccache,
  coverage tooling.
