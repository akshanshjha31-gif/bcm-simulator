/**
 * @file    test_ring_buffer.cpp
 * @brief   Unit tests for drivers::RingBuffer (backs the UART queues).
 *
 * Traces SRS-COM-001 and SRS-REL-002 (no dynamic allocation).
 */
#include "catch2/catch.hpp"
#include "ring_buffer.h"

using bcm::drivers::RingBuffer;

TEST_CASE("A new buffer is empty", "[ringbuffer]")
{
    RingBuffer<uint8_t, 4U> rb;
    REQUIRE(rb.empty());
    REQUIRE_FALSE(rb.full());
    REQUIRE(rb.size() == 0U);
    REQUIRE(rb.capacity() == 4U);
}

TEST_CASE("Popping an empty buffer fails and leaves the output alone", "[ringbuffer]")
{
    RingBuffer<uint8_t, 4U> rb;

    uint8_t out = 0xAAU;
    REQUIRE_FALSE(rb.pop(out));
    REQUIRE(out == 0xAAU);
}

TEST_CASE("Items come back in FIFO order", "[ringbuffer]")
{
    RingBuffer<uint8_t, 8U> rb;

    for (uint8_t i = 0U; i < 5U; ++i) { REQUIRE(rb.push(i)); }
    REQUIRE(rb.size() == 5U);

    for (uint8_t i = 0U; i < 5U; ++i) {
        uint8_t out = 0U;
        REQUIRE(rb.pop(out));
        REQUIRE(out == i);
    }
    REQUIRE(rb.empty());
}

TEST_CASE("Pushing to a full buffer is rejected, not silently overwriting", "[ringbuffer]")
{
    RingBuffer<uint8_t, 4U> rb;

    for (uint8_t i = 0U; i < 4U; ++i) { REQUIRE(rb.push(i)); }
    REQUIRE(rb.full());

    REQUIRE_FALSE(rb.push(99U));   // dropped, not clobbering the oldest
    REQUIRE(rb.size() == 4U);

    uint8_t out = 0U;
    REQUIRE(rb.pop(out));
    REQUIRE(out == 0U);            // oldest survived intact
}

TEST_CASE("Indices wrap correctly under sustained use", "[ringbuffer]")
{
    RingBuffer<uint8_t, 4U> rb;

    /* Push/pop far past capacity so head and tail wrap many times. */
    for (uint16_t i = 0U; i < 1000U; ++i) {
        const uint8_t v = static_cast<uint8_t>(i & 0xFFU);
        REQUIRE(rb.push(v));

        uint8_t out = 0U;
        REQUIRE(rb.pop(out));
        REQUIRE(out == v);
    }
    REQUIRE(rb.empty());
}

TEST_CASE("Partial drain then refill keeps ordering", "[ringbuffer]")
{
    RingBuffer<uint8_t, 4U> rb;

    rb.push(1U); rb.push(2U); rb.push(3U); rb.push(4U);

    uint8_t out = 0U;
    rb.pop(out); REQUIRE(out == 1U);
    rb.pop(out); REQUIRE(out == 2U);

    REQUIRE(rb.push(5U));
    REQUIRE(rb.push(6U));
    REQUIRE(rb.full());

    rb.pop(out); REQUIRE(out == 3U);
    rb.pop(out); REQUIRE(out == 4U);
    rb.pop(out); REQUIRE(out == 5U);
    rb.pop(out); REQUIRE(out == 6U);
    REQUIRE(rb.empty());
}

TEST_CASE("peek() shows the oldest item without removing it", "[ringbuffer]")
{
    RingBuffer<uint8_t, 4U> rb;
    rb.push(7U);
    rb.push(8U);

    uint8_t out = 0U;
    REQUIRE(rb.peek(out));
    REQUIRE(out == 7U);
    REQUIRE(rb.size() == 2U);   // unchanged
}

TEST_CASE("clear() empties the buffer", "[ringbuffer]")
{
    RingBuffer<uint8_t, 4U> rb;
    rb.push(1U); rb.push(2U);

    rb.clear();
    REQUIRE(rb.empty());
    REQUIRE(rb.size() == 0U);

    REQUIRE(rb.push(9U));
    uint8_t out = 0U;
    REQUIRE(rb.pop(out));
    REQUIRE(out == 9U);
}

TEST_CASE("Works with non-byte payloads", "[ringbuffer]")
{
    struct Frame { uint8_t cmd; uint16_t len; };
    RingBuffer<Frame, 3U> rb;

    REQUIRE(rb.push(Frame{ 0x10U, 4U }));
    REQUIRE(rb.push(Frame{ 0x20U, 8U }));

    Frame f{};
    REQUIRE(rb.pop(f));
    REQUIRE(f.cmd == 0x10U);
    REQUIRE(f.len == 4U);
}
