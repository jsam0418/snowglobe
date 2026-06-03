cmake_minimum_required(VERSION 3.25)

project(snowglobe
    VERSION 0.1.0
    DESCRIPTION "Server-authoritative 2D physics engine streamed over WebSockets"
    LANGUAGES CXX)

# Presets supply the build type; default to Release for bare `cmake -S . -B ...`.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SNOWGLOBE_WERROR "Treat warnings as errors for Snowglobe's own targets" ON)
option(SNOWGLOBE_BUILD_TESTS "Build unit tests" ON)
option(SNOWGLOBE_BUILD_BENCHMARKS "Build microbenchmarks" ON)
set(SNOWGLOBE_SANITIZER "" CACHE STRING
    "Sanitizers to enable on project targets, e.g. 'address;undefined' or 'thread'")

find_package(Threads REQUIRED)

# --------------------------------------------------------------------------- #
# snowglobe_project_options: warning flags, -Werror, and sanitizers, applied
# ONLY to our own targets via target_link_libraries(... PRIVATE ...).
# Dependencies (uWS, gtest, benchmark) never link this, so a warning in a
# dependency header can never break our build.
# --------------------------------------------------------------------------- #
add_library(snowglobe_project_options INTERFACE)
target_compile_options(snowglobe_project_options INTERFACE
    -Wall -Wextra -Wpedantic
    -Wshadow -Wconversion -Wsign-conversion
    -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused
    -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wformat=2)
target_compile_options(snowglobe_project_options INTERFACE
    $<$<BOOL:${SNOWGLOBE_WERROR}>:-Werror>)

if(SNOWGLOBE_SANITIZER)
    string(REPLACE ";" "," _snowglobe_san "${SNOWGLOBE_SANITIZER}")
    message(STATUS "Snowglobe sanitizers: ${_snowglobe_san}")
    target_compile_options(snowglobe_project_options INTERFACE
        -fsanitize=${_snowglobe_san} -fno-omit-frame-pointer -g)
    target_link_options(snowglobe_project_options INTERFACE
        -fsanitize=${_snowglobe_san})
endif()

# --------------------------------------------------------------------------- #
# snowglobe_core: header-only simulation core for v1 (SPSC queue, ECS storage).
# Dependency include dirs from imported targets are SYSTEM by default, so uWS
# headers pulled in transitively do not trigger our strict warnings.
# --------------------------------------------------------------------------- #
add_library(snowglobe_core INTERFACE)
target_include_directories(snowglobe_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(snowglobe_core INTERFACE Threads::Threads)

# --------------------------------------------------------------------------- #
# snowglobe: the WebSocket server / simulation host.
# --------------------------------------------------------------------------- #
find_package(unofficial-uwebsockets CONFIG REQUIRED)

add_executable(snowglobe src/main.cpp)
target_link_libraries(snowglobe PRIVATE
    snowglobe_core
    snowglobe_project_options
    unofficial::uwebsockets::uwebsockets)

if(SNOWGLOBE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(SNOWGLOBE_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
