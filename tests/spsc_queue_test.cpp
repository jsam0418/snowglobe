#include "snowglobe/core/spsc_queue.hpp"

#include <cstdint>
#include <gtest/gtest.h>
#include <thread>

namespace {

using snowglobe::core::SpscQueue;

// Pops a value the test expects to be present, failing the test (rather than
// throwing) if the queue is unexpectedly empty.
int pop_expected(SpscQueue<int>& queue) {
    auto value = queue.try_pop();
    EXPECT_TRUE(value.has_value());
    return value.value_or(-1);
}

TEST(SpscQueue, PopFromEmptyReturnsNullopt) {
    SpscQueue<int> queue(4);
    EXPECT_FALSE(queue.try_pop().has_value());
}

TEST(SpscQueue, FifoOrderAndFullness) {
    SpscQueue<int> queue(4); // usable capacity = 3
    EXPECT_TRUE(queue.try_push(1));
    EXPECT_TRUE(queue.try_push(2));
    EXPECT_TRUE(queue.try_push(3));
    EXPECT_FALSE(queue.try_push(4)); // full

    EXPECT_EQ(pop_expected(queue), 1);
    EXPECT_EQ(pop_expected(queue), 2);
    EXPECT_TRUE(queue.try_push(4)); // slot freed
    EXPECT_EQ(pop_expected(queue), 3);
    EXPECT_EQ(pop_expected(queue), 4);
    EXPECT_FALSE(queue.try_pop().has_value());
}

// Cross-thread hand-off — this is the case the `debug-tsan` preset exercises.
TEST(SpscQueue, ConcurrentProducerConsumerPreservesEverything) {
    constexpr std::uint64_t kCount = 100'000;
    SpscQueue<std::uint64_t> queue(1024);

    std::thread producer([&] {
        for (std::uint64_t i = 0; i < kCount;) {
            if (queue.try_push(i)) {
                ++i;
            }
        }
    });

    std::uint64_t received = 0;
    std::uint64_t expected_next = 0;
    while (received < kCount) {
        if (auto value = queue.try_pop()) {
            ASSERT_EQ(*value, expected_next); // FIFO must hold
            ++expected_next;
            ++received;
        }
    }

    producer.join();
    EXPECT_EQ(received, kCount);
}

} // namespace
