// Toolchain smoke test: confirms GoogleTest, CTest discovery, and the sanitizer
// presets (debug-asan / debug-tsan) build and run. Replace with real tests as
// the engine is designed.

#include <atomic>
#include <gtest/gtest.h>
#include <thread>

namespace {

TEST(Smoke, GoogleTestRuns) {
    EXPECT_EQ(2 + 2, 4);
}

// A trivial threaded check so the `debug-tsan` preset actually exercises
// ThreadSanitizer at runtime (no data race expected).
TEST(Smoke, ThreadSanitizerHasSomethingToCheck) {
    std::atomic<int> counter{0};
    std::thread a([&] { counter.fetch_add(1, std::memory_order_relaxed); });
    std::thread b([&] { counter.fetch_add(1, std::memory_order_relaxed); });
    a.join();
    b.join();
    EXPECT_EQ(counter.load(std::memory_order_relaxed), 2);
}

} // namespace
