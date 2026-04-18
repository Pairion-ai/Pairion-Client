#pragma once

/**
 * @file ring_buffer.h
 * @brief Lock-free single-producer single-consumer (SPSC) ring buffer.
 *
 * Header-only template with power-of-two capacity for fast modulo via bitmask.
 * Designed for the audio pipeline: the capture thread pushes PCM frames, and the
 * downstream consumer (encoder/wake/VAD) pops them without locking.
 */

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace pairion::audio {

/**
 * @brief Lock-free SPSC ring buffer with compile-time fixed capacity.
 *
 * @tparam T Element type (must be default-constructible and move-assignable).
 * @tparam Capacity Buffer size; must be a power of two.
 *
 * Memory ordering: the producer uses release stores on the write index;
 * the consumer uses acquire loads on the write index. This ensures that
 * the element written by the producer is visible to the consumer before
 * the index update.
 */
template <typename T, std::size_t Capacity>
class RingBuffer {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                  "RingBuffer capacity must be a power of two");

  public:
    /**
     * @brief Push an element into the buffer.
     * @param item Element to enqueue.
     * @return true if pushed, false if buffer is full.
     */
    bool push(const T &item) {
        const std::size_t w = m_writeIdx.load(std::memory_order_relaxed);
        const std::size_t nextW = (w + 1) & kMask;
        if (nextW == m_readIdx.load(std::memory_order_acquire)) {
            return false; // full
        }
        m_data[w] = item;
        m_writeIdx.store(nextW, std::memory_order_release);
        return true;
    }

    /**
     * @brief Pop an element from the buffer.
     * @param item Output parameter receiving the dequeued element.
     * @return true if popped, false if buffer is empty.
     */
    bool pop(T &item) {
        const std::size_t r = m_readIdx.load(std::memory_order_relaxed);
        if (r == m_writeIdx.load(std::memory_order_acquire)) {
            return false; // empty
        }
        item = std::move(m_data[r]);
        m_readIdx.store((r + 1) & kMask, std::memory_order_release);
        return true;
    }

    /**
     * @brief Check if the buffer is empty.
     */
    bool isEmpty() const {
        return m_readIdx.load(std::memory_order_acquire) ==
               m_writeIdx.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if the buffer is full.
     */
    bool isFull() const {
        const std::size_t w = m_writeIdx.load(std::memory_order_acquire);
        return ((w + 1) & kMask) == m_readIdx.load(std::memory_order_acquire);
    }

    /**
     * @brief Number of elements currently in the buffer.
     */
    std::size_t size() const {
        const std::size_t w = m_writeIdx.load(std::memory_order_acquire);
        const std::size_t r = m_readIdx.load(std::memory_order_acquire);
        return (w - r) & kMask;
    }

  private:
    static constexpr std::size_t kMask = Capacity - 1;
    std::array<T, Capacity> m_data{};
    std::atomic<std::size_t> m_writeIdx{0};
    std::atomic<std::size_t> m_readIdx{0};
};

} // namespace pairion::audio
