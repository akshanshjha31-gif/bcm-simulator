/**
 * @file    ring_buffer.h
 * @brief   Fixed-capacity FIFO - pure logic, no HAL, no dynamic allocation.
 *
 * Backs the UART driver's TX/RX queues. Storage is a member array sized at
 * compile time, so nothing is ever heap-allocated on a real-time path
 * (SRS-REL-002, coding standard in the architecture doc).
 *
 * Concurrency: safe for exactly one producer and one consumer (typically an
 * ISR on one side and a task on the other), because push() only advances
 * head_ and pop() only advances tail_. Anything beyond that needs external
 * locking.
 */
#ifndef BCM_RING_BUFFER_H
#define BCM_RING_BUFFER_H

#include <stdint.h>

namespace bcm {
namespace drivers {

template <typename T, uint16_t Capacity>
class RingBuffer {
    static_assert(Capacity > 1U, "RingBuffer needs capacity > 1");

public:
    RingBuffer() : head_(0U), tail_(0U), count_(0U) {}

    /// @return false if the buffer was full (item dropped).
    bool push(const T& item)
    {
        if (count_ >= Capacity) { return false; }
        buf_[head_] = item;
        head_ = next(head_);
        ++count_;
        return true;
    }

    /// @return false if the buffer was empty (out untouched).
    bool pop(T& out)
    {
        if (count_ == 0U) { return false; }
        out = buf_[tail_];
        tail_ = next(tail_);
        --count_;
        return true;
    }

    /// Look at the oldest item without removing it.
    bool peek(T& out) const
    {
        if (count_ == 0U) { return false; }
        out = buf_[tail_];
        return true;
    }

    void clear() { head_ = 0U; tail_ = 0U; count_ = 0U; }

    bool     empty() const { return count_ == 0U; }
    bool     full() const { return count_ >= Capacity; }
    uint16_t size() const { return count_; }
    uint16_t capacity() const { return Capacity; }

private:
    static uint16_t next(uint16_t i)
    {
        return static_cast<uint16_t>((i + 1U) % Capacity);
    }

    T                 buf_[Capacity];
    volatile uint16_t head_;
    volatile uint16_t tail_;
    volatile uint16_t count_;
};

}  // namespace drivers
}  // namespace bcm

#endif /* BCM_RING_BUFFER_H */
