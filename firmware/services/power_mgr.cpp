/**
 * @file    power_mgr.cpp
 * @brief   Power state machine and load shedding.
 */
#include "power_mgr.h"

namespace bcm {
namespace services {
namespace {

typedef FsmTransition<PowerState, PowerEvent, PowerContext> PowerRow;

/* Wake and Shutdown are short transient states rather than instant jumps, so
 * there is a defined place to run POST on the way up and to park outputs on
 * the way down. */
const PowerRow kTable[] = {
    /* from                   any    event                          to                     guard action */
    { PowerState::Sleep,      false, PowerEvent::IgnitionOn,        PowerState::Wake,     0, 0 },
    { PowerState::Wake,       false, PowerEvent::WakeComplete,      PowerState::Run,      0, 0 },

    { PowerState::Run,        false, PowerEvent::IgnitionOff,       PowerState::Shutdown, 0, 0 },
    { PowerState::Wake,       false, PowerEvent::IgnitionOff,       PowerState::Shutdown, 0, 0 },
    { PowerState::Shutdown,   false, PowerEvent::ShutdownComplete,  PowerState::Sleep,    0, 0 },

    /* Ignition back on during shutdown aborts it - the driver wins. */
    { PowerState::Shutdown,   false, PowerEvent::IgnitionOn,        PowerState::Run,      0, 0 },

    { PowerState::Run,        false, PowerEvent::InactivityElapsed, PowerState::Shutdown, 0, 0 },
};

const uint8_t  kRows           = sizeof(kTable) / sizeof(kTable[0]);
const uint32_t kWakeMs         = 200U;
const uint32_t kShutdownMs     = 500U;

}  // namespace

PowerMgr::PowerMgr(const Config& config)
    : cfg_(config),
      fsm_(kTable, kRows, PowerState::Sleep),
      ctx_(),
      /* Hysteresis is on "battery OK": rises above ok_permille to clear the
       * shed, falls below low_permille to assert it. */
      battery_ok_(config.battery_low_permille, config.battery_ok_permille, true),
      transition_(),
      inactivity_(),
      battery_known_(false)
{
}

void PowerMgr::ignition_on()
{
    if (fsm_.dispatch(PowerEvent::IgnitionOn, ctx_)) {
        if (fsm_.state() == PowerState::Wake) { transition_.start(kWakeMs); }
        if (fsm_.state() == PowerState::Run)  { inactivity_.start(cfg_.sleep_after_ms); }
    }
}

void PowerMgr::ignition_off()
{
    if (fsm_.dispatch(PowerEvent::IgnitionOff, ctx_)) {
        transition_.start(kShutdownMs);
        inactivity_.stop();
    }
}

void PowerMgr::update_battery(uint16_t permille)
{
    const bool ok = battery_ok_.update(permille);

    /* Ignore the very first reading for shedding purposes: the ADC filter has
     * not settled and a spurious shed at power-up would blank the lamps. */
    if (!battery_known_) {
        battery_known_ = true;
        return;
    }
    ctx_.load_shed = !ok;
}

void PowerMgr::update(uint32_t dt_ms)
{
    if (transition_.update(dt_ms)) {
        if (fsm_.state() == PowerState::Wake) {
            fsm_.dispatch(PowerEvent::WakeComplete, ctx_);
            inactivity_.start(cfg_.sleep_after_ms);
        }
        else if (fsm_.state() == PowerState::Shutdown) {
            fsm_.dispatch(PowerEvent::ShutdownComplete, ctx_);
        }
    }

    if (inactivity_.update(dt_ms)) {
        if (fsm_.dispatch(PowerEvent::InactivityElapsed, ctx_)) {
            transition_.start(kShutdownMs);
        }
    }
}

void PowerMgr::apply(LampState& out) const
{
    if (fsm_.state() == PowerState::Run || fsm_.state() == PowerState::Wake) {
        out.set(LampId::Ignition, LampDrive::On);
    }
}

void PowerMgr::reset()
{
    fsm_.set_state(PowerState::Sleep);
    ctx_ = PowerContext();
    transition_.stop();
    inactivity_.stop();
    battery_known_ = false;
}

}  // namespace services
}  // namespace bcm
