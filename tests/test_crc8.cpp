/**
 * @file    test_crc8.cpp
 * @brief   Unit tests for services::Crc8 (BCM-ICD-001 frame check sequence).
 */
#include "catch2/catch.hpp"
#include "crc8.h"

using bcm::services::Crc8;

TEST_CASE("CRC of no data is the initial value", "[crc8]")
{
    Crc8 crc;
    REQUIRE(crc.value() == Crc8::kInitial);
}

TEST_CASE("CRC-8/ATM matches known vectors", "[crc8]")
{
    /* Reference values for polynomial 0x07, init 0x00, no reflection.
     * These pin the wire format down so the C# tool can be written against
     * them independently. */
    /* "123456789" -> 0xF4 is the published check value for this polynomial,
     * so it validates the implementation against the standard rather than
     * against itself. */
    const uint8_t digits[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    REQUIRE(Crc8::compute(digits, 9U) == 0xF4U);

    const uint8_t abc[] = { 'A', 'B', 'C' };
    REQUIRE(Crc8::compute(abc, 3U) == 0x52U);

    const uint8_t zero = 0x00U;
    REQUIRE(Crc8::compute(&zero, 1U) == 0x00U);
}

TEST_CASE("CRC detects any single-bit error", "[crc8]")
{
    uint8_t data[] = { 0x04U, 0x02U, 0x03U, 0x01U };
    const uint8_t good = Crc8::compute(data, 4U);

    for (uint16_t byte = 0U; byte < 4U; ++byte) {
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            data[byte] ^= static_cast<uint8_t>(1U << bit);
            REQUIRE(Crc8::compute(data, 4U) != good);
            data[byte] ^= static_cast<uint8_t>(1U << bit);   // restore
        }
    }
}

TEST_CASE("CRC detects byte reordering", "[crc8]")
{
    /* This is the case a plain XOR checksum cannot see at all. */
    const uint8_t a[] = { 0x01U, 0x02U, 0x03U };
    const uint8_t b[] = { 0x03U, 0x02U, 0x01U };

    REQUIRE(Crc8::compute(a, 3U) != Crc8::compute(b, 3U));
}

TEST_CASE("CRC detects a two-bit error in the same column", "[crc8]")
{
    /* Also invisible to XOR: two flips at the same bit position. */
    uint8_t data[] = { 0x10U, 0x10U, 0x55U };
    const uint8_t good = Crc8::compute(data, 3U);

    data[0] ^= 0x01U;
    data[1] ^= 0x01U;
    REQUIRE(Crc8::compute(data, 3U) != good);
}

TEST_CASE("Incremental update matches the one-shot helper", "[crc8]")
{
    const uint8_t data[] = { 0xAAU, 0x55U, 0x00U, 0xFFU, 0x7EU };

    Crc8 crc;
    for (uint8_t b : data) { crc.update(b); }

    REQUIRE(crc.value() == Crc8::compute(data, 5U));
}

TEST_CASE("reset() returns the CRC to its initial state", "[crc8]")
{
    Crc8 crc;
    crc.update(0x42U);
    crc.reset();

    REQUIRE(crc.value() == Crc8::kInitial);
}

TEST_CASE("A null pointer is ignored rather than dereferenced", "[crc8]")
{
    Crc8 crc;
    crc.update(static_cast<const uint8_t*>(0), 10U);
    REQUIRE(crc.value() == Crc8::kInitial);
}
