/**
 * @file    test_icd_examples.cpp
 * @brief   Executable conformance check for BCM-ICD-001 section 5.
 *
 * The worked examples in the ICD are the contract the C# desktop tool will be
 * written against. Asserting the exact bytes here means the document cannot
 * drift away from the firmware unnoticed - if someone changes the framing or
 * the polynomial, these fail immediately.
 */
#include "catch2/catch.hpp"
#include "frame_codec.h"
#include "protocol.h"

#include <vector>

using bcm::services::Cmd;
using bcm::services::Frame;
using bcm::services::FrameCodec;
using bcm::services::FrameParser;
using bcm::services::kMaxFrameSize;
using bcm::services::kResponseFlag;
using bcm::services::make_status_response;
using bcm::services::Status;

namespace {

std::vector<uint8_t> on_the_wire(const Frame& f)
{
    uint8_t  buf[kMaxFrameSize];
    uint16_t written = 0U;
    REQUIRE(FrameCodec::encode(f, buf, sizeof(buf), written));
    return std::vector<uint8_t>(buf, buf + written);
}

Frame request(uint8_t cmd, std::initializer_list<uint8_t> bytes)
{
    Frame f;
    f.cmd = cmd;
    f.len = static_cast<uint8_t>(bytes.size());
    uint8_t i = 0U;
    for (uint8_t b : bytes) { f.payload[i++] = b; }
    return f;
}

}  // namespace

TEST_CASE("ICD 5.1 - PING request bytes", "[icd]")
{
    const std::vector<uint8_t> expected = { 0xAAU, 0x01U, 0x00U, 0x15U, 0x55U };
    REQUIRE(on_the_wire(request(static_cast<uint8_t>(Cmd::Ping), {})) == expected);
}

TEST_CASE("ICD 5.1 - PING response bytes", "[icd]")
{
    const Frame resp = make_status_response(static_cast<uint8_t>(Cmd::Ping),
                                            Status::Ok);
    const std::vector<uint8_t> expected = { 0xAAU, 0x81U, 0x01U, 0x00U, 0x75U, 0x55U };
    REQUIRE(on_the_wire(resp) == expected);
}

TEST_CASE("ICD 5.2 - SET_LAMP request bytes", "[icd]")
{
    /* Brake lamp (id 7) on. */
    const std::vector<uint8_t> expected =
        { 0xAAU, 0x04U, 0x02U, 0x07U, 0x01U, 0xE2U, 0x55U };
    REQUIRE(on_the_wire(request(static_cast<uint8_t>(Cmd::SetLamp), { 0x07U, 0x01U }))
            == expected);
}

TEST_CASE("ICD 5.2 - SET_LAMP response bytes", "[icd]")
{
    const Frame resp = make_status_response(static_cast<uint8_t>(Cmd::SetLamp),
                                            Status::Ok);
    const std::vector<uint8_t> expected = { 0xAAU, 0x84U, 0x01U, 0x00U, 0xB5U, 0x55U };
    REQUIRE(on_the_wire(resp) == expected);
}

TEST_CASE("ICD 5.3 - malformed request and its rejection", "[icd]")
{
    const std::vector<uint8_t> req_expected =
        { 0xAAU, 0x04U, 0x01U, 0x07U, 0xABU, 0x55U };
    REQUIRE(on_the_wire(request(static_cast<uint8_t>(Cmd::SetLamp), { 0x07U }))
            == req_expected);

    const Frame resp = make_status_response(static_cast<uint8_t>(Cmd::SetLamp),
                                            Status::BadLength);
    const std::vector<uint8_t> resp_expected =
        { 0xAAU, 0x84U, 0x01U, 0x02U, 0xBBU, 0x55U };
    REQUIRE(on_the_wire(resp) == resp_expected);
}

TEST_CASE("ICD 3 - frame size bounds match the document", "[icd]")
{
    Frame empty;
    REQUIRE(FrameCodec::encoded_size(empty) == 5U);

    Frame full;
    full.len = bcm::services::kMaxPayload;
    REQUIRE(FrameCodec::encoded_size(full) == 37U);
}

TEST_CASE("ICD 4 - the response flag is bit 7", "[icd]")
{
    REQUIRE(kResponseFlag == 0x80U);

    const Frame resp = make_status_response(static_cast<uint8_t>(Cmd::GetVersion),
                                            Status::Ok);
    REQUIRE(resp.cmd == 0x82U);
}

TEST_CASE("ICD 5 - documented bytes decode back to the same frame", "[icd]")
{
    /* Round-trip the literal ICD bytes through the parser, proving a host
     * that copies them verbatim will be understood. */
    const std::vector<uint8_t> wire =
        { 0xAAU, 0x04U, 0x02U, 0x07U, 0x01U, 0xE2U, 0x55U };

    FrameParser p;
    FrameParser::Result outcome = FrameParser::Result::NeedMore;
    for (uint8_t b : wire) {
        const FrameParser::Result r = p.feed(b);
        if (r != FrameParser::Result::NeedMore) { outcome = r; }
    }

    REQUIRE(outcome == FrameParser::Result::Complete);
    REQUIRE(p.frame().cmd == static_cast<uint8_t>(Cmd::SetLamp));
    REQUIRE(p.frame().len == 2U);
    REQUIRE(p.frame().payload[0] == 0x07U);
    REQUIRE(p.frame().payload[1] == 0x01U);
}
