#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <optional>
#include <vector>

namespace snowglobe::core {

// Bounded, wait-free single-producer / single-consumer ring buffer.
//
// This is the hand-off between the uWebSockets I/O thread and the simulation
// thread: exactly one thread calls try_push() and exactly one (different)
// thread calls try_pop(). Correctness of the acquire/release ordering here is
// what the `debug-tsan` preset exists to validate — run the unit tests under
// ThreadSanitizer after touching this file.
template <typename T> class SpscQueue {
  public:
    explicit SpscQueue(std::size_t capacity)
        // One slot is reserved to distinguish "full" from "empty", so usable
        // capacity is capacity - 1.
        : buffer_(capacity), capacity_(capacity) {}

    // Producer side only.
    [[nodiscard]] bool try_push(T value) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = increment(head);
        if (next == tail_.load(std::memory_order_acquire)) {
            return false; // full
        }
        buffer_[head] = std::move(value);
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Consumer side only.
    [[nodiscard]] std::optional<T> try_pop() {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return std::nullopt; // empty
        }
        T value = std::move(buffer_[tail]);
        tail_.store(increment(tail), std::memory_order_release);
        return value;
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_ - 1; }

  private:
    [[nodiscard]] std::size_t increment(std::size_t index) const noexcept {
        return (index + 1) % capacity_;
    }

#ifdef __cpp_lib_hardware_interference_size
    static constexpr std::size_t kCacheLine = std::hardware_destructive_interference_size;
#else
    static constexpr std::size_t kCacheLine = 64;
#endif

    std::vector<T> buffer_;
    std::size_t capacity_;
    // Keep the two cursors on separate cache lines to avoid false sharing.
    alignas(kCacheLine) std::atomic<std::size_t> head_{0};
    alignas(kCacheLine) std::atomic<std::size_t> tail_{0};
};

} // namespace snowglobe::core
