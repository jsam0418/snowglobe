# syntax=docker/dockerfile:1

# --------------------------------------------------------------------------- #
# Snowglobe — single image for development, CI, and production.
#
# Rationale (see docs/FRAMEWORKS.md): one image carries the full toolchain
# (Clang, CMake, Ninja, vcpkg + resolved deps, clang-tidy/format, GoogleTest,
# Google Benchmark) so dev, CI, and prod share an identical environment with
# zero toolchain drift. Tradeoff: the prod image is larger than a slim runtime.
# Revisit a multi-stage build if image size / hardening becomes a concern.
#
# Multi-arch: builds on both arm64 (the dev Raspberry Pi / cluster) and amd64.
# vcpkg builds every dependency from source, so nothing here is arch-pinned.
# --------------------------------------------------------------------------- #
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Toolchain + the bits vcpkg needs to build dependencies from source.
# g++-14 / libstdc++-14 is installed so Clang picks up the newest libstdc++
# (our chosen standard library — see CLAUDE.md) with the best C++23 coverage.
RUN apt-get update && apt-get install -y --no-install-recommends \
        clang-18 \
        clang-tidy-18 \
        clang-format-18 \
        lld-18 \
        llvm-18 \
        libclang-rt-18-dev \
        g++-14 \
        libstdc++-14-dev \
        ninja-build \
        git \
        curl zip unzip tar \
        pkg-config \
        ca-certificates \
        python3 \
        python3-pip \
        autoconf automake libtool \
    && rm -rf /var/lib/apt/lists/*

# Ubuntu 24.04 ships CMake 3.28, but recent vcpkg ports (e.g. usockets) require
# CMake >= 3.30. Install a current CMake via pip (arch-agnostic wheels) so it
# takes precedence on PATH over any system cmake. Pinned for zero drift; bump
# deliberately when a vcpkg port needs newer.
RUN pip3 install --no-cache-dir --break-system-packages "cmake==4.3.2"

# Make the unversioned tool names resolve to the Clang 18 binaries.
RUN update-alternatives --install /usr/bin/clang        clang        /usr/bin/clang-18        100 \
 && update-alternatives --install /usr/bin/clang++      clang++      /usr/bin/clang++-18      100 \
 && update-alternatives --install /usr/bin/clang-tidy   clang-tidy   /usr/bin/clang-tidy-18   100 \
 && update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-18 100 \
 && update-alternatives --install /usr/bin/ld.lld       ld.lld       /usr/bin/ld.lld-18       100

# Build everything (including vcpkg deps and the project) with Clang.
ENV CC=clang
ENV CXX=clang++

# --------------------------------------------------------------------------- #
# vcpkg (manifest mode). Pin VCPKG_REF to a release tag/commit for a
# reproducible dependency set: docker build --build-arg VCPKG_REF=<tag>.
# --------------------------------------------------------------------------- #
ARG VCPKG_REF=master
ENV VCPKG_ROOT=/opt/vcpkg
ENV VCPKG_DEFAULT_BINARY_CACHE=/opt/vcpkg-cache
# arm64 has no vcpkg-shipped cmake/ninja; use the system ones we installed.
ENV VCPKG_FORCE_SYSTEM_BINARIES=1

RUN git clone https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}" \
 && git -C "${VCPKG_ROOT}" checkout "${VCPKG_REF}" \
 && "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics \
 && mkdir -p "${VCPKG_DEFAULT_BINARY_CACHE}"
ENV PATH="${VCPKG_ROOT}:${PATH}"

# Pre-build the dependency set so it lands in the binary cache. Project builds
# inside the container then restore prebuilt archives instead of recompiling
# uWebSockets/GoogleTest/Benchmark from scratch. This layer re-runs only when
# vcpkg.json changes.
WORKDIR /opt/deps-prefetch
COPY vcpkg.json ./
RUN vcpkg install --clean-after-build

WORKDIR /workspace

# Bake the application into the image so the SAME image is the deployable
# artifact (the toolchain stays present for dev / CI use). Source changes only
# invalidate from this COPY down — the heavy dependency layers above stay
# cached. vcpkg restores the prebuilt deps from the binary cache, so this is a
# quick compile of just our own code.
COPY . .
RUN cmake --preset release \
 && cmake --build build/release --target snowglobe

ENV SNOWGLOBE_PORT=9001
EXPOSE 9001

# Production runs the prebuilt server. For dev / CI, override the command and
# bind-mount the working tree over /workspace to iterate, e.g.:
#   docker run --rm -it -v "$PWD":/workspace snowglobe:dev bash
CMD ["./build/release/snowglobe"]
