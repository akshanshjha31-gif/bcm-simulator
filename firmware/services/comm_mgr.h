/**
 * @file    comm_mgr.h
 * @brief   Communication manager: transport -> parser -> dispatcher -> reply.
 *
 * Depends on an ITransport abstraction rather than drv_uart directly, so the
 * whole receive/dispatch/respond path is exercised on the host with a fake
 * transport (architecture document section 3.1, "D": services depend on
 * abstractions).
 *
 * Also owns the link-loss timer behind SRS-SAFETY-007: if no valid frame
 * arrives within the timeout, the link is declared lost so the application
 * can drive outputs to a defined safe state.
 */
#ifndef BCM_COMM_MGR_H
#define BCM_COMM_MGR_H

#include "protocol.h"
#include "frame_codec.h"
#include "dispatcher.h"
#include "soft_timer.h"

namespace bcm {
namespace services {

/**
 * @brief Byte-stream transport abstraction (UART on target, a fake in tests).
 */
class ITransport {
public:
    virtual ~ITransport() {}

    /// Pop one received byte. False when nothing is pending.
    virtual bool read_byte(uint8_t& out) = 0;

    /// Queue bytes for transmission. False if they could not be accepted.
    virtual bool write(const uint8_t* data, uint16_t len) = 0;
};

class CommMgr {
public:
    /// No valid frame for this long means the host has gone away.
    static const uint32_t kDefaultTimeoutMs = 2000U;

    CommMgr(ITransport& transport,
            Dispatcher& dispatcher,
            uint32_t    timeout_ms = kDefaultTimeoutMs);

    /**
     * @brief Drain the transport, decode, dispatch and reply.
     * @param dt_ms milliseconds since the previous call.
     * @param budget maximum bytes to consume this pass, so a flood cannot
     *               starve the rest of the super-loop.
     * @return number of frames handled this pass.
     */
    uint8_t poll(uint16_t dt_ms, uint16_t budget = 64U);

    /// Send an unsolicited frame (events, heartbeats).
    bool send(const Frame& frame);

    /// True once the timeout has elapsed with no valid frame (SRS-SAFETY-007).
    bool link_lost() const { return link_lost_; }

    uint32_t frames_ok() const { return parser_.frames_ok(); }
    uint32_t frames_bad() const { return parser_.frames_bad(); }
    uint32_t tx_failures() const { return tx_failures_; }

private:
    ITransport&        transport_;
    Dispatcher&        dispatcher_;
    FrameParser        parser_;
    drivers::SoftTimer link_timer_;
    uint32_t           timeout_ms_;
    uint32_t           tx_failures_;
    bool               link_lost_;
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_COMM_MGR_H */
