/**
 * @file    bcm_core.cpp
 * @brief   The complete BCM control cycle, free of any hardware.
 */
#include "bcm_core.h"

namespace bcm {
namespace services {

BcmCore::BcmCore(const Config& config)
    : cfg_(config),
      lighting_(config),
      door_(config),
      power_(config),
      horn_(config),
      faults_(),
      log_(),
      inputs_(),
      previous_(),
      lamps_(),
      host_request_(),
      host_mask_(0U),
      comms_lost_(false),
      host_lock_(false),
      host_unlock_(false),
      host_light_mode_(false),
      ignition_held_ms_(0U),
      long_press_fired_(false),
      prev_load_shed_(false),
      prev_comms_lost_(false),
      prev_door_(DoorState::Locked),
      first_cycle_(true)
{
}

void BcmCore::set_host_request(const LampState& request, uint16_t mask)
{
    host_request_ = request;
    host_mask_    = mask;
}

void BcmCore::apply_edges(uint16_t dt_ms)
{
    /* On the very first cycle every input looks like a rising edge, which
     * would fire the indicators the instant the ECU powers up. Seed the
     * history instead. */
    if (first_cycle_) {
        previous_    = inputs_;
        first_cycle_ = false;
        return;
    }

    const bool ign_rise  = inputs_.ignition && !previous_.ignition;
    const bool ign_fall  = !inputs_.ignition && previous_.ignition;
    (void)ign_rise;

    /* ---- Ignition: short press toggles power, long press steps lights. */
    if (inputs_.ignition) {
        ignition_held_ms_ = static_cast<uint16_t>(ignition_held_ms_ + dt_ms);
        if (!long_press_fired_ && ignition_held_ms_ >= cfg_.long_press_ms) {
            long_press_fired_ = true;
            lighting_.next_mode();
            log_.log(0U, LogEvent::LightMode,
                     static_cast<uint8_t>(lighting_.light_state()));
        }
    }
    if (ign_fall) {
        if (!long_press_fired_) {
            if (power_.running()) {
                power_.ignition_off();
                log_.log(0U, LogEvent::IgnitionOff);
                /* Ignition off must not leave the headlights burning. */
                lighting_.all_off();
            }
            else {
                power_.ignition_on();
                log_.log(0U, LogEvent::IgnitionOn);
            }
        }
        ignition_held_ms_ = 0U;
        long_press_fired_ = false;
    }

    /* ---- Indicators ---------------------------------------------------- */
    if (inputs_.ind_left && !previous_.ind_left) {
        lighting_.indicator_left();
        log_.log(0U, LogEvent::IndicatorLeft);
    }
    if (inputs_.ind_right && !previous_.ind_right) {
        lighting_.indicator_right();
        log_.log(0U, LogEvent::IndicatorRight);
    }
    if (inputs_.hazard && !previous_.hazard) {
        lighting_.indicator_hazard();
        log_.log(0U, (lighting_.indicator_state() == IndState::Hazard)
                         ? LogEvent::HazardOn : LogEvent::HazardOff);
    }

    /* ---- Doors ---------------------------------------------------------- */
    door_.set_door_open(inputs_.door_open);

    if (inputs_.door_lock && !previous_.door_lock) {
        if (door_.locked()) { door_.request_unlock(); }
        else                { door_.request_lock(); }
    }
    if (host_lock_)   { host_lock_ = false;   door_.request_lock(); }
    if (host_unlock_) { host_unlock_ = false; door_.request_unlock(); }

    if (host_light_mode_) {
        host_light_mode_ = false;
        lighting_.next_mode();
    }

    /* ---- Brake, logged on change --------------------------------------- */
    if (inputs_.brake != previous_.brake) {
        log_.log(0U, inputs_.brake ? LogEvent::BrakeApplied
                                   : LogEvent::BrakeReleased);
    }

    previous_ = inputs_;
}

void BcmCore::step(uint16_t dt_ms)
{
    apply_edges(dt_ms);

    /* ---- Sensors into the managers -------------------------------------- */
    lighting_.update_ambient(inputs_.ambient_permille);
    power_.update_battery(inputs_.battery_permille);

    power_.update(dt_ms);
    door_.update(dt_ms);

    /* ---- Faults and one-shots ------------------------------------------- */
    if (door_.take_lock_refused()) {
        faults_.set(Dtc::LockRefusedDoorOpen);
        log_.log(0U, LogEvent::LockRefused);
    }
    if (door_.take_chirp_lock())   { horn_.chirp(1U); }
    if (door_.take_chirp_unlock()) { horn_.chirp(2U); }

    if (power_.load_shed()) { faults_.set(Dtc::BatteryLow); }
    else                    { faults_.clear(Dtc::BatteryLow); }

    if (comms_lost_) { faults_.set(Dtc::CommsLost); }
    else             { faults_.clear(Dtc::CommsLost); }

    if (power_.load_shed() != prev_load_shed_) {
        prev_load_shed_ = power_.load_shed();
        log_.log(0U, prev_load_shed_ ? LogEvent::BatteryLow : LogEvent::BatteryOk);
    }
    if (comms_lost_ != prev_comms_lost_) {
        prev_comms_lost_ = comms_lost_;
        log_.log(0U, prev_comms_lost_ ? LogEvent::CommsLost
                                      : LogEvent::CommsRestored);
    }
    if (door_.state() != prev_door_) {
        prev_door_ = door_.state();
        log_.log(0U, (prev_door_ == DoorState::Locked) ? LogEvent::DoorLocked
                                                       : LogEvent::DoorUnlocked);
    }

    /* ---- Collect what every manager wants lit --------------------------- */
    LampState wanted;
    power_.apply(wanted);
    lighting_.apply(wanted);
    door_.apply(wanted);

    /* ---- Merge diagnostic-host requests, BEFORE arbitration ------------- */
    for (uint8_t i = 0U; i < static_cast<uint8_t>(LampId::Count); ++i) {
        if ((host_mask_ & (1U << i)) != 0U) {
            wanted.drive[i] = host_request_.drive[i];
        }
    }

    /* ---- Arbitrate: the last word before anything reaches a pin --------- */
    ArbiterInputs arb;
    arb.brake_pressed   = inputs_.brake;
    arb.reverse_gear    = inputs_.reverse_gear;
    arb.lamps_permitted = power_.lamps_permitted();
    arb.load_shed       = power_.load_shed();
    arb.comms_lost      = comms_lost_;
    LampArbiter::arbitrate(wanted, arb);

    lamps_ = wanted;

    (void)horn_.update(dt_ms);
}

void BcmCore::reset()
{
    lighting_.reset();
    door_.reset();
    power_.reset();
    horn_.reset();
    faults_.clear_all();
    log_.clear();

    inputs_           = Inputs();
    previous_         = Inputs();
    lamps_            = LampState();
    host_mask_        = 0U;
    comms_lost_       = false;
    host_lock_        = false;
    host_unlock_      = false;
    host_light_mode_  = false;
    ignition_held_ms_ = 0U;
    long_press_fired_ = false;
    prev_load_shed_   = false;
    prev_comms_lost_  = false;
    prev_door_        = DoorState::Locked;
    first_cycle_      = true;
}

}  // namespace services
}  // namespace bcm
