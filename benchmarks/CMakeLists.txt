find_package(benchmark CONFIG REQUIRED)

add_executable(snowglobe_benchmarks
    smoke_bench.cpp)

target_link_libraries(snowglobe_benchmarks PRIVATE
    snowglobe_core
    snowglobe_project_options
    benchmark::benchmark
    benchmark::benchmark_main)
