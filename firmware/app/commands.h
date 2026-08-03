/**
 * @file    commands.h
 * @brief   Concrete command handlers for the diagnostic protocol.
 *
 * The dispatcher table lives here, in the composition root, because the
 * handlers are the only part of the protocol that needs to know about actual
 * lamps and sensors. Framing and routing stay generic and hardware-free.
 *
 * Host lamp requests are recorded rather than written straight to the pins:
 * the application merges them BEFORE arbitration, so a diagnostic host can
 * exercise a lamp but cannot talk its way past a safety rule.
 */
#ifndef BCM_COMMANDS_H
#define BCM_COMMANDS_H

#include "bcm_types.h"
#include "dispatcher.h"
#include "drv_adc.h"
#include "fault_mgr.h"

namespace bcm {
namespace app {

/// Everything the handlers are allowed to touch.
struct CommandContext {
    /// Lamp states the host has asked for, merged pre-arbitration.
    services::LampState*  host_request;
    /// Bit per LampId: which lamps the host is currently driving.
    uint16_t              host_mask;

    /// Live lamp state after arbitration, for GET_STATUS / GET_LAMP.
    const services::LampState* actual;

    drivers::AnalogInput* battery;
    services::FaultMgr*   faults;
    uint8_t               switch_bitmap;
    uint8_t               power_state;

    /* Requests raised by handlers and acted on by the application, which owns
     * the FSMs. A handler must not reach into a state machine directly. */
    bool                  reset_requested;
    bool                  door_lock_requested;
    bool                  door_unlock_requested;

    CommandContext()
        : host_request(0), host_mask(0U), actual(0), battery(0), faults(0),
          switch_bitmap(0U), power_state(0U), reset_requested(false),
          door_lock_requested(false), door_unlock_requested(false)
    {
    }
};

/// Dispatcher table and its length (defined in commands.cpp).
const services::CommandEntry* command_table();
uint8_t                       command_table_size();

}  // namespace app
}  // namespace bcm

#endif /* BCM_COMMANDS_H */
