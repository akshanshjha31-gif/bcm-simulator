/**
 * @file    power_mgr.h
 * @brief   Power state machine and load shedding - pure logic, no HAL.
 *
 * Sleep -> Wake -> Run -> Shutdown -> Sleep
 *
 * Also owns battery supervision: below a threshold the BCM sheds
 * non-essential load, and recovery uses a higher threshold so a sagging
 * battery cannot oscillate (SRS-PWR-004).
 *
 * Traces SRS-PWR-001..004, SRS-SAFETY-002.
 */
#ifndef BCM_POWER_MGR_H
#define BCM_POWER_MGR_H

#include "bcm_config.h"
#include "bcm_types.h"
#include "filter.h"
#include "fsm.h"
#include "soft_timer.h"

namespace bcm {
namespace services {

enum class PowerState : uint8_t {
    Sleep = 0,
    Wake,
    Run,
    Shutdown
};

enum class PowerEvent : uint8_t {
    IgnitionOn,
    IgnitionOff,
    WakeComplete,
    ShutdownComplete,
    InactivityElapsed
};

struct PowerContext {
    bool load_shed;      ///< battery low: non-essential lamps suppressed

    PowerContext() : load_shed(false) {}
};

class PowerMgr {
public:
    explicit PowerMgr(const Config& config);

    void ignition_on();
    void ignition_off();

    /// Feed the filtered battery reading (per mille of full scale).
    void update_battery(uint16_t permille);

    /// Advance wake/shutdown/inactivity timers.
    void update(uint32_t dt_ms);

    void apply(LampState& out) const;

    PowerState state() const { return fsm_.state(); }
    bool       running() const { return fsm_.state() == PowerState::Run; }

    /// True while the battery is below threshold and load is being shed.
    bool load_shed() const { return ctx_.load_shed; }

    /// In Sleep, only lamps explicitly permitted may be lit (SRS-PWR-003).
    bool lamps_permitted() const { return fsm_.state() != PowerState::Sleep; }

    void reset();

private:
    const Config&                                   cfg_;
    Fsm<PowerState, PowerEvent, PowerContext>       fsm_;
    PowerContext                                    ctx_;
    drivers::Hysteresis                             battery_ok_;
    drivers::SoftTimer                              transition_;
    drivers::SoftTimer                              inactivity_;
    bool                                            battery_known_;
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_POWER_MGR_H */
