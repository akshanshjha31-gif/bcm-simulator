/**
 * @file    test_dispatcher.cpp
 * @brief   Unit tests for services::Dispatcher and services::CommMgr.
 *
 * Traces SRS-COM-003 (command set) and SRS-SAFETY-007 (communication
 * timeout drives a defined safe state).
 */
#include "catch2/catch.hpp"
#include "comm_mgr.h"
#include "dispatcher.h"
#include "frame_codec.h"

#include <deque>
#include <vector>

using bcm::services::Cmd;
using bcm::services::CommandEntry;
using bcm::services::CommMgr;
using bcm::services::Dispatcher;
using bcm::services::Frame;
using bcm::services::FrameCodec;
using bcm::services::FrameParser;
using bcm::services::ITransport;
using bcm::services::kMaxFrameSize;
using bcm::services::kResponseFlag;
using bcm::services::Status;

namespace {

struct TestContext {
    int      ping_calls = 0;
    int      lamp_calls = 0;
    uint8_t  last_lamp  = 0xFFU;
    bool     last_state = false;
};

bool handle_ping(const Frame& req, Frame& resp, void* ctx)
{
    static_cast<TestContext*>(ctx)->ping_calls++;
    /* Echo the payload back after the status byte. */
    resp.len = static_cast<uint8_t>(1U + req.len);
    for (uint8_t i = 0U; i < req.len; ++i) { resp.payload[1U + i] = req.payload[i]; }
    return true;
}

bool handle_set_lamp(const Frame& req, Frame& resp, void* ctx)
{
    TestContext* c = static_cast<TestContext*>(ctx);
    c->lamp_calls++;

    if (req.payload[0] > 8U) {
        resp.payload[0] = static_cast<uint8_t>(Status::BadParameter);
        return true;
    }
    c->last_lamp  = req.payload[0];
    c->last_state = (req.payload[1] != 0U);
    return true;
}

bool handle_fire_and_forget(const Frame&, Frame&, void*) { return false; }

const CommandEntry kTable[] = {
    { static_cast<uint8_t>(Cmd::Ping),      handle_ping,             0U },
    { static_cast<uint8_t>(Cmd::SetLamp),   handle_set_lamp,         2U },
    { static_cast<uint8_t>(Cmd::Reset),     handle_fire_and_forget,  0U },
};
const uint8_t kTableCount = 3U;

Frame request(Cmd cmd, std::initializer_list<uint8_t> bytes = {})
{
    Frame f;
    f.cmd = static_cast<uint8_t>(cmd);
    f.len = static_cast<uint8_t>(bytes.size());
    uint8_t i = 0U;
    for (uint8_t b : bytes) { f.payload[i++] = b; }
    return f;
}

/// In-memory transport: RX is a queue the test fills, TX is captured.
class FakeTransport : public ITransport {
public:
    bool read_byte(uint8_t& out) override
    {
        if (rx.empty()) { return false; }
        out = rx.front();
        rx.pop_front();
        return true;
    }

    bool write(const uint8_t* data, uint16_t len) override
    {
        if (fail_writes) { return false; }
        for (uint16_t i = 0U; i < len; ++i) { tx.push_back(data[i]); }
        return true;
    }

    void queue(const Frame& f)
    {
        uint8_t  buf[kMaxFrameSize];
        uint16_t written = 0U;
        REQUIRE(FrameCodec::encode(f, buf, sizeof(buf), written));
        for (uint16_t i = 0U; i < written; ++i) { rx.push_back(buf[i]); }
    }

    std::deque<uint8_t>  rx;
    std::vector<uint8_t> tx;
    bool                 fail_writes = false;
};

/// Decode the first frame sitting in the transmit capture.
bool decode_tx(const FakeTransport& t, Frame& out)
{
    FrameParser p;
    for (uint8_t b : t.tx) {
        if (p.feed(b) == FrameParser::Result::Complete) {
            out = p.frame();
            return true;
        }
    }
    return false;
}

}  // namespace

TEST_CASE("A known command reaches its handler", "[dispatcher]")
{
    TestContext ctx;
    Dispatcher d(kTable, kTableCount, &ctx);

    Frame resp;
    REQUIRE(d.dispatch(request(Cmd::Ping), resp));
    REQUIRE(ctx.ping_calls == 1);
    REQUIRE(resp.payload[0] == static_cast<uint8_t>(Status::Ok));
}

TEST_CASE("A response carries the request id with the response flag", "[dispatcher]")
{
    TestContext ctx;
    Dispatcher d(kTable, kTableCount, &ctx);

    Frame resp;
    d.dispatch(request(Cmd::Ping), resp);
    REQUIRE(resp.cmd == (static_cast<uint8_t>(Cmd::Ping) | kResponseFlag));
}

TEST_CASE("An unknown command is answered, not ignored", "[dispatcher]")
{
    /* Silence would be indistinguishable from a dead link. */
    TestContext ctx;
    Dispatcher d(kTable, kTableCount, &ctx);

    Frame req;
    req.cmd = 0x7EU;
    req.len = 0U;

    Frame resp;
    REQUIRE(d.dispatch(req, resp));
    REQUIRE(resp.payload[0] == static_cast<uint8_t>(Status::UnknownCmd));
    REQUIRE(d.rejected() == 1U);
}

TEST_CASE("A short payload is rejected before the handler runs", "[dispatcher]")
{
    TestContext ctx;
    Dispatcher d(kTable, kTableCount, &ctx);

    Frame resp;
    REQUIRE(d.dispatch(request(Cmd::SetLamp, { 0x01U }), resp));   // needs 2
    REQUIRE(resp.payload[0] == static_cast<uint8_t>(Status::BadLength));
    REQUIRE(ctx.lamp_calls == 0);   // handler never saw a malformed request
}

TEST_CASE("A handler can reject its parameters", "[dispatcher]")
{
    TestContext ctx;
    Dispatcher d(kTable, kTableCount, &ctx);

    Frame resp;
    d.dispatch(request(Cmd::SetLamp, { 0x63U, 0x01U }), resp);
    REQUIRE(resp.payload[0] == static_cast<uint8_t>(Status::BadParameter));
}

TEST_CASE("Handler arguments arrive intact", "[dispatcher]")
{
    TestContext ctx;
    Dispatcher d(kTable, kTableCount, &ctx);

    Frame resp;
    d.dispatch(request(Cmd::SetLamp, { 0x03U, 0x01U }), resp);
    REQUIRE(ctx.last_lamp == 0x03U);
    REQUIRE(ctx.last_state == true);
}

TEST_CASE("A response frame is never dispatched as a request", "[dispatcher]")
{
    /* Guards against a looped-back TX line making the BCM answer itself. */
    TestContext ctx;
    Dispatcher d(kTable, kTableCount, &ctx);

    Frame req;
    req.cmd = static_cast<uint8_t>(Cmd::Ping) | kResponseFlag;
    req.len = 0U;

    Frame resp;
    REQUIRE_FALSE(d.dispatch(req, resp));
    REQUIRE(ctx.ping_calls == 0);
}

TEST_CASE("A fire-and-forget handler suppresses the reply", "[dispatcher]")
{
    TestContext ctx;
    Dispatcher d(kTable, kTableCount, &ctx);

    Frame resp;
    REQUIRE_FALSE(d.dispatch(request(Cmd::Reset), resp));
}

TEST_CASE("An empty table answers everything as unknown", "[dispatcher]")
{
    Dispatcher d(0, 0U, 0);

    Frame resp;
    REQUIRE(d.dispatch(request(Cmd::Ping), resp));
    REQUIRE(resp.payload[0] == static_cast<uint8_t>(Status::UnknownCmd));
}

TEST_CASE("CommMgr decodes a request and transmits the reply", "[comm]")
{
    TestContext   ctx;
    Dispatcher    d(kTable, kTableCount, &ctx);
    FakeTransport t;
    CommMgr       comm(t, d);

    t.queue(request(Cmd::Ping, { 0xABU }));
    REQUIRE(comm.poll(1U) == 1U);

    Frame reply;
    REQUIRE(decode_tx(t, reply));
    REQUIRE(reply.cmd == (static_cast<uint8_t>(Cmd::Ping) | kResponseFlag));
    REQUIRE(reply.payload[0] == static_cast<uint8_t>(Status::Ok));
    REQUIRE(reply.payload[1] == 0xABU);       // echoed
}

TEST_CASE("CommMgr handles several frames in one pass", "[comm]")
{
    TestContext   ctx;
    Dispatcher    d(kTable, kTableCount, &ctx);
    FakeTransport t;
    CommMgr       comm(t, d);

    for (int i = 0; i < 3; ++i) { t.queue(request(Cmd::Ping)); }
    REQUIRE(comm.poll(1U) == 3U);
    REQUIRE(ctx.ping_calls == 3);
}

TEST_CASE("The byte budget bounds work per pass", "[comm]")
{
    TestContext   ctx;
    Dispatcher    d(kTable, kTableCount, &ctx);
    FakeTransport t;
    CommMgr       comm(t, d);

    /* A flooded link must not starve the rest of the super-loop. */
    for (int i = 0; i < 10; ++i) { t.queue(request(Cmd::Ping)); }

    /* A zero-payload frame is exactly 5 bytes (HDR CMD LEN CKSUM FTR), so the
     * budget has to be below that to stop mid-frame. */
    REQUIRE(comm.poll(1U, 3U) == 0U);
    REQUIRE_FALSE(t.rx.empty());        // remainder still pending

    /* And the partial frame must complete on the following pass. */
    REQUIRE(comm.poll(1U, 64U) >= 1U);
}

TEST_CASE("Corrupt input is counted and does not produce a reply", "[comm]")
{
    TestContext   ctx;
    Dispatcher    d(kTable, kTableCount, &ctx);
    FakeTransport t;
    CommMgr       comm(t, d);

    t.queue(request(Cmd::Ping, { 0x01U }));
    t.rx[3] ^= 0xFFU;                   // corrupt the payload

    REQUIRE(comm.poll(1U) == 0U);
    REQUIRE(t.tx.empty());
    REQUIRE(comm.frames_bad() == 1U);
}

TEST_CASE("The link is declared lost after the timeout", "[comm]")
{
    /* SRS-SAFETY-007: the application needs a defined signal to fall back on. */
    TestContext   ctx;
    Dispatcher    d(kTable, kTableCount, &ctx);
    FakeTransport t;
    CommMgr       comm(t, d, 1000U);

    REQUIRE_FALSE(comm.link_lost());

    comm.poll(999U);
    REQUIRE_FALSE(comm.link_lost());

    comm.poll(1U);
    REQUIRE(comm.link_lost());
}

TEST_CASE("A valid frame restores a lost link", "[comm]")
{
    TestContext   ctx;
    Dispatcher    d(kTable, kTableCount, &ctx);
    FakeTransport t;
    CommMgr       comm(t, d, 1000U);

    comm.poll(2000U);
    REQUIRE(comm.link_lost());

    t.queue(request(Cmd::Ping));
    comm.poll(1U);
    REQUIRE_FALSE(comm.link_lost());
}

TEST_CASE("A failing transport is counted rather than silently dropped", "[comm]")
{
    TestContext   ctx;
    Dispatcher    d(kTable, kTableCount, &ctx);
    FakeTransport t;
    CommMgr       comm(t, d);

    t.fail_writes = true;
    t.queue(request(Cmd::Ping));
    comm.poll(1U);

    REQUIRE(comm.tx_failures() == 1U);
}
