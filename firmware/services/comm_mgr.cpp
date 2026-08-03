/**
 * @file    comm_mgr.cpp
 * @brief   Communication manager.
 */
#include "comm_mgr.h"

namespace bcm {
namespace services {

CommMgr::CommMgr(ITransport& transport,
                 Dispatcher& dispatcher,
                 uint32_t    timeout_ms)
    : transport_(transport),
      dispatcher_(dispatcher),
      parser_(),
      link_timer_(),
      timeout_ms_(timeout_ms),
      tx_failures_(0U),
      link_lost_(false)
{
    link_timer_.start(timeout_ms_);
}

bool CommMgr::send(const Frame& frame)
{
    uint8_t  buffer[kMaxFrameSize];
    uint16_t written = 0U;

    if (!FrameCodec::encode(frame, buffer, sizeof(buffer), written)) {
        ++tx_failures_;
        return false;
    }
    if (!transport_.write(buffer, written)) {
        ++tx_failures_;
        return false;
    }
    return true;
}

uint8_t CommMgr::poll(uint16_t dt_ms, uint16_t budget)
{
    uint8_t handled = 0U;

    for (uint16_t i = 0U; i < budget; ++i) {
        uint8_t byte = 0U;
        if (!transport_.read_byte(byte)) { break; }

        if (parser_.feed(byte) != FrameParser::Result::Complete) { continue; }

        /* A valid frame proves the host is alive, whatever it asked for. */
        link_timer_.start(timeout_ms_);
        link_lost_ = false;

        Frame response;
        if (dispatcher_.dispatch(parser_.frame(), response)) {
            (void)send(response);
        }
        ++handled;
    }

    /* Only arm the loss condition while it is not already latched, so the
     * one-shot is not restarted every pass once the link is down. */
    if (!link_lost_ && link_timer_.update(dt_ms)) {
        link_lost_ = true;
    }

    return handled;
}

}  // namespace services
}  // namespace bcm
