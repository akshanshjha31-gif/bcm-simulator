/**
 * @file    bcm_core.h
 * @brief   The complete BCM control cycle, free of any hardware - the piece
 *          the firmware and the SIL harness genuinely share.
 *
 * Takes a snapshot of already-debounced inputs, runs every state machine,
 * applies arbitration, and publishes the lamp and horn outputs. It never
 * touches a pin, a register or the HAL, so the exact same object runs:
 *
 *   - on the target, driven by BcmApp from real GPIO and ADC readings;
 *   - on the host, driven by the SIL runner from a scripted timeline.
 *
 * That is what makes the SIL worth anything: a scenario that passes there is
 * exercising the shipped logic, not a model of it.
 */
#ifndef BCM_CORE_H
#define BCM_CORE_H

#include "bcm_config.h"
#include "bcm_types.h"
#include "door_mgr.h"
#include "fault_mgr.h"
#include "horn_mgr.h"
#include "lamp_arbiter.h"
#include "lighting_mgr.h"
#include "logger.h"
#include "power_mgr.h"

namespace bcm {
namespace services {

class BcmCore {
public:
    explicit BcmCore(const Config& config);

    /// Supply this cycle's debounced switch levels and filtered analog values.
    void set_inputs(const Inputs& in) { inputs_ = in; }

    /// Tell the core whether the diagnostic link is currently down.
    void set_comms_lost(bool lost) { comms_lost_ = lost; }

    /// Lamp states requested by the diagnostic host, merged before
    /// arbitration so a host can exercise a lamp but not defeat a rule.
    void set_host_request(const LampState& request, uint16_t mask);

    /* Requests arriving from the diagnostic link rather than a switch. */
    void request_door_lock()   { host_lock_ = true; }
    void request_door_unlock() { host_unlock_ = true; }
    void request_light_mode()  { host_light_mode_ = true; }

    /// Run one control cycle.
    void step(uint16_t dt_ms);

    /* ---- Outputs --------------------------------------------------------- */

    const LampState& lamps() const { return lamps_; }
    bool             horn() const { return horn_.output(); }

    /* ---- Observable state, for diagnostics and for SIL assertions -------- */

    LightState light_state() const { return lighting_.light_state(); }
    IndState   indicator_state() const { return lighting_.indicator_state(); }
    DoorState  door_state() const { return door_.state(); }
    PowerState power_state() const { return power_.state(); }
    bool       load_shed() const { return power_.load_shed(); }

    FaultMgr&       faults() { return faults_; }
    const FaultMgr& faults() const { return faults_; }
    Logger&         log() { return log_; }

    void reset();

private:
    void apply_edges(uint16_t dt_ms);

    const Config& cfg_;
    LightingMgr   lighting_;
    DoorMgr       door_;
    PowerMgr      power_;
    HornMgr       horn_;
    FaultMgr      faults_;
    Logger        log_;

    Inputs        inputs_;
    Inputs        previous_;
    LampState     lamps_;
    LampState     host_request_;
    uint16_t      host_mask_;
    bool          comms_lost_;

    bool          host_lock_;
    bool          host_unlock_;
    bool          host_light_mode_;

    /* Ignition doubles as the light switch until one is wired to PB5:
     * a short press toggles power, holding it steps the lighting mode. */
    uint16_t      ignition_held_ms_;
    bool          long_press_fired_;

    /* Previous values, so events are logged on change rather than every
     * cycle - a 5 ms loop would otherwise flood the log instantly. */
    bool          prev_load_shed_;
    bool          prev_comms_lost_;
    DoorState     prev_door_;
    bool          first_cycle_;
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_CORE_H */
