/**
 * @file    test_frame_codec.cpp
 * @brief   Unit tests for services::FrameCodec and services::FrameParser.
 *
 * Traces SRS-COM-001 (framed protocol) and SRS-COM-002 (corrupt frames are
 * rejected, and the link resynchronises rather than desynchronising).
 */
#include "catch2/catch.hpp"
#include "frame_codec.h"

#include <vector>

using bcm::services::Frame;
using bcm::services::FrameCodec;
using bcm::services::FrameParser;
using bcm::services::kFrameFooter;
using bcm::services::kFrameHeader;
using bcm::services::kMaxFrameSize;
using bcm::services::kMaxPayload;

namespace {

Frame make(uint8_t cmd, std::initializer_list<uint8_t> bytes)
{
    Frame f;
    f.cmd = cmd;
    f.len = static_cast<uint8_t>(bytes.size());
    uint8_t i = 0U;
    for (uint8_t b : bytes) { f.payload[i++] = b; }
    return f;
}

std::vector<uint8_t> encode(const Frame& f)
{
    uint8_t  buf[kMaxFrameSize];
    uint16_t written = 0U;
    REQUIRE(FrameCodec::encode(f, buf, sizeof(buf), written));
    return std::vector<uint8_t>(buf, buf + written);
}

/// Feed a whole buffer and report the last conclusive outcome.
///
/// Errors are raised part-way through a frame, and the bytes after them are
/// consumed during resynchronisation and answer NeedMore - so simply
/// returning the final call's result would hide the failure.
FrameParser::Result feed_all(FrameParser& p, const std::vector<uint8_t>& bytes)
{
    FrameParser::Result outcome = FrameParser::Result::NeedMore;
    for (uint8_t b : bytes) {
        const FrameParser::Result r = p.feed(b);
        if (r != FrameParser::Result::NeedMore) { outcome = r; }
    }
    return outcome;
}

}  // namespace

TEST_CASE("An encoded frame has the documented layout", "[codec]")
{
    const Frame f = make(0x04U, { 0x02U, 0x01U });
    const std::vector<uint8_t> wire = encode(f);

    REQUIRE(wire.size() == 7U);            // HDR CMD LEN P P CKSUM FTR
    REQUIRE(wire.front() == kFrameHeader);
    REQUIRE(wire[1] == 0x04U);             // CMD
    REQUIRE(wire[2] == 0x02U);             // LEN
    REQUIRE(wire[3] == 0x02U);             // payload
    REQUIRE(wire[4] == 0x01U);
    REQUIRE(wire.back() == kFrameFooter);
}

TEST_CASE("A zero-length payload encodes and decodes", "[codec]")
{
    const Frame f = make(0x01U, {});
    const std::vector<uint8_t> wire = encode(f);
    REQUIRE(wire.size() == 5U);

    FrameParser p;
    REQUIRE(feed_all(p, wire) == FrameParser::Result::Complete);
    REQUIRE(p.frame().cmd == 0x01U);
    REQUIRE(p.frame().len == 0U);
}

TEST_CASE("Encode rejects an oversized payload", "[codec]")
{
    Frame f;
    f.cmd = 0x01U;
    f.len = static_cast<uint8_t>(kMaxPayload + 1U);

    uint8_t  buf[kMaxFrameSize];
    uint16_t written = 0xFFFFU;
    REQUIRE_FALSE(FrameCodec::encode(f, buf, sizeof(buf), written));
    REQUIRE(written == 0U);
}

TEST_CASE("Encode rejects a buffer that is too small", "[codec]")
{
    const Frame f = make(0x01U, { 1U, 2U, 3U });

    uint8_t  buf[4];
    uint16_t written = 0xFFFFU;
    REQUIRE_FALSE(FrameCodec::encode(f, buf, sizeof(buf), written));
    REQUIRE(written == 0U);
}

TEST_CASE("Encode rejects a null buffer", "[codec]")
{
    const Frame f = make(0x01U, {});
    uint16_t written = 0xFFFFU;
    REQUIRE_FALSE(FrameCodec::encode(f, 0, 16U, written));
}

TEST_CASE("A round trip preserves cmd and payload", "[codec]")
{
    const Frame sent = make(0x42U, { 0xDEU, 0xADU, 0xBEU, 0xEFU });

    FrameParser p;
    REQUIRE(feed_all(p, encode(sent)) == FrameParser::Result::Complete);

    REQUIRE(p.frame().cmd == sent.cmd);
    REQUIRE(p.frame().len == sent.len);
    for (uint8_t i = 0U; i < sent.len; ++i) {
        REQUIRE(p.frame().payload[i] == sent.payload[i]);
    }
    REQUIRE(p.frames_ok() == 1U);
    REQUIRE(p.frames_bad() == 0U);
}

TEST_CASE("A maximum-size payload round trips", "[codec]")
{
    Frame f;
    f.cmd = 0x33U;
    f.len = kMaxPayload;
    for (uint8_t i = 0U; i < kMaxPayload; ++i) { f.payload[i] = i; }

    FrameParser p;
    REQUIRE(feed_all(p, encode(f)) == FrameParser::Result::Complete);
    REQUIRE(p.frame().len == kMaxPayload);
    REQUIRE(p.frame().payload[kMaxPayload - 1U] == kMaxPayload - 1U);
}

TEST_CASE("Payload bytes equal to HDR or FTR are carried transparently", "[codec]")
{
    /* Payloads are not escaped, so this is the case that would break a naive
     * parser scanning for the footer. */
    const Frame f = make(0x07U, { kFrameHeader, kFrameFooter, kFrameHeader });

    FrameParser p;
    REQUIRE(feed_all(p, encode(f)) == FrameParser::Result::Complete);
    REQUIRE(p.frame().len == 3U);
    REQUIRE(p.frame().payload[0] == kFrameHeader);
    REQUIRE(p.frame().payload[1] == kFrameFooter);
    REQUIRE(p.frame().payload[2] == kFrameHeader);
}

TEST_CASE("A corrupted payload is rejected by the checksum", "[codec]")
{
    std::vector<uint8_t> wire = encode(make(0x04U, { 0x02U, 0x01U }));
    wire[3] ^= 0x01U;                        // flip one payload bit

    FrameParser p;
    REQUIRE(feed_all(p, wire) == FrameParser::Result::ChecksumError);
    REQUIRE(p.frames_ok() == 0U);
    REQUIRE(p.frames_bad() == 1U);
}

TEST_CASE("A wrong footer is a framing error", "[codec]")
{
    std::vector<uint8_t> wire = encode(make(0x04U, { 0x02U }));
    wire.back() = 0x00U;

    FrameParser p;
    REQUIRE(feed_all(p, wire) == FrameParser::Result::FramingError);
    REQUIRE(p.frames_bad() == 1U);
}

TEST_CASE("An impossible length is rejected immediately", "[codec]")
{
    FrameParser p;
    p.feed(kFrameHeader);
    p.feed(0x01U);                                    // CMD
    REQUIRE(p.feed(static_cast<uint8_t>(kMaxPayload + 1U)) ==
            FrameParser::Result::Overflow);
    REQUIRE(p.frames_bad() == 1U);
}

TEST_CASE("Leading noise before a frame is discarded", "[codec]")
{
    std::vector<uint8_t> stream = { 0x00U, 0xFFU, 0x12U, 0x55U, 0x99U };
    const std::vector<uint8_t> wire = encode(make(0x02U, { 0x07U }));
    stream.insert(stream.end(), wire.begin(), wire.end());

    FrameParser p;
    REQUIRE(feed_all(p, stream) == FrameParser::Result::Complete);
    REQUIRE(p.frame().cmd == 0x02U);
}

TEST_CASE("The parser resynchronises after a corrupt frame", "[codec]")
{
    /* This is the property that matters on a real link: one bad frame must
     * cost exactly one frame, not the whole session. */
    std::vector<uint8_t> bad = encode(make(0x04U, { 0x01U }));
    bad[3] ^= 0xFFU;

    const std::vector<uint8_t> good = encode(make(0x05U, { 0x09U }));

    FrameParser p;
    REQUIRE(feed_all(p, bad) == FrameParser::Result::ChecksumError);
    REQUIRE(feed_all(p, good) == FrameParser::Result::Complete);
    REQUIRE(p.frame().cmd == 0x05U);
    REQUIRE(p.frames_ok() == 1U);
    REQUIRE(p.frames_bad() == 1U);
}

TEST_CASE("Back-to-back frames are decoded without gaps", "[codec]")
{
    std::vector<uint8_t> stream;
    for (uint8_t i = 0U; i < 5U; ++i) {
        const std::vector<uint8_t> w = encode(make(static_cast<uint8_t>(0x10U + i), { i }));
        stream.insert(stream.end(), w.begin(), w.end());
    }

    FrameParser p;
    int completed = 0;
    for (uint8_t b : stream) {
        if (p.feed(b) == FrameParser::Result::Complete) { ++completed; }
    }
    REQUIRE(completed == 5);
    REQUIRE(p.frames_ok() == 5U);
}

TEST_CASE("Truncation costs at most one following frame", "[codec]")
{
    std::vector<uint8_t> truncated = encode(make(0x04U, { 1U, 2U, 3U }));
    truncated.resize(4U);                       // cut mid-payload

    FrameParser p;
    feed_all(p, truncated);

    /* The abandoned frame is still expecting payload bytes, so it swallows
     * the next header and that frame is lost. What must NOT happen is
     * permanent desynchronisation - the link has to come back by itself. */
    int completed = 0;
    for (int i = 0; i < 2; ++i) {
        if (feed_all(p, encode(make(0x06U, {}))) == FrameParser::Result::Complete) {
            ++completed;
        }
    }
    REQUIRE(completed >= 1);
    REQUIRE(p.frame().cmd == 0x06U);
}

TEST_CASE("Statistics survive a reset()", "[codec]")
{
    FrameParser p;
    feed_all(p, encode(make(0x01U, {})));
    REQUIRE(p.frames_ok() == 1U);

    p.reset();
    REQUIRE(p.frames_ok() == 1U);   // counters are not part of the sync state
}
