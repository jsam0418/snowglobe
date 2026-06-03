# Snowglobe — Development Environment & Tooling Spec

> Project name: **Snowglobe**. Use `snowglobe` as the repo name, container image name, and Kubernetes namespace. Public URL stays `game.josiahsam.com`.

## Overview
A C++23 2D physics engine, server-authoritative, that streams simulation state over WebSockets to a browser sandbox. Runs headless (no GPU) as a container on a Kubernetes cluster; rendering happens client-side in each viewer's browser. This document is the environment/tooling spec only — not the application design.

## Toolchain
- **Compiler:** Clang 18+ targeting **C++23**.
- **Standard library:** pick **libc++ _or_ libstdc++** and keep it consistent across local and container builds.
- **Modules:** do **not** use C++23 modules / `import std;` for v1 — still rough in Clang/libc++. Use all other C++23 language and library features freely.

## Build System
- **CMake with presets** (`CMakePresets.json`).
- `CMAKE_EXPORT_COMPILE_COMMANDS=ON` (drives clangd + clang-tidy).
- Preset triplet:
  - `release` — optimized build.
  - `debug-asan` — Debug + AddressSanitizer + UndefinedBehaviorSanitizer.
  - `debug-tsan` — Debug + ThreadSanitizer (**required** — validates the lock-free SPSC queue between the I/O thread and the sim thread).

## Dependencies
- **vcpkg in manifest mode** (`vcpkg.json`) as the single dependency manager.
  - `uWebSockets` — WebSocket server + event loop (pulls in uSockets and its C deps).
  - `gtest` (GoogleTest) — unit testing.
  - `benchmark` (Google Benchmark) — microbenchmarks for hot-path measurement (e.g. SoA vs AoS).

## Code Quality
- **clang-format** with a committed `.clang-format`; enforced as a CI check.
- **clang-tidy** with a committed `.clang-tidy`.
- **Warnings:** `-Wall -Wextra -Wpedantic` (and related).
- **`-Werror` scoped to the project's own targets only** — never applied to dependency headers (a warning in a uWS header must not break the build).
- **Sanitizers:** ASan + UBSan (`debug-asan`) and TSan (`debug-tsan`) via the presets above.

## Testing & Benchmarking
- **GoogleTest** for unit tests.
- **Google Benchmark** for microbenchmarks of the simulation hot paths.

## Editor / LSP
- **clangd**, fed by the exported `compile_commands.json`.

## Source Control & CI/CD
- **Public Git repo (GitHub)** — portfolio-facing, shown to potential employers. GitHub is the single source of truth (no Gitea mirror).
- **GitHub Actions** pipeline (`.github/workflows/ci.yml`): configure → build → clang-format check → clang-tidy → unit tests → sanitizer builds. The whole pipeline runs *inside* the project image so CI tests the exact artifact it ships.
- **Container registry: GHCR.** On `master`, CI builds the image multi-arch and pushes it to `ghcr.io/jsam0418/snowglobe:<git-sha>` (immutable SHA tags) using the built-in `GITHUB_TOKEN`.
- **Flux** deploys to Kubernetes from the GitHub repo (GitOps). Manifests live in `deploy/` (a Kustomize overlay); `clusters/production/` holds the Flux `GitRepository` + `Kustomization`. After pushing the image, CI commits the new tag into `deploy/kustomization.yaml` ("Strategy A"); Flux reconciles it and the cluster pulls from GHCR.

## Containerization
- **Single image for both development and production** (decision: keep it simple, identical environment).
- Image contains the full toolchain: Clang, CMake, vcpkg + resolved dependencies, clang-tidy, clang-format, GoogleTest, Google Benchmark.
- Used as the local devcontainer **and** as the deployed artifact → **zero toolchain drift** between dev, CI, and prod.
- _Known tradeoff:_ the prod image carries build tooling, so it is larger and has a broader surface than a slim runtime. Acceptable for a v1 personal sandbox; revisit a multi-stage build with a minimal runtime stage later if image size or hardening becomes a concern.

## Out of Scope for v1
Add only if a concrete pain appears:
- include-what-you-use (IWYU)
- scan-build / static analysis beyond clang-tidy
- ccache
- coverage tooling
- C++23 modules / `import std;`
