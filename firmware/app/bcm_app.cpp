/**
 * @file    bcm_app.cpp
 * @brief   Application composition root.
 */
#include "bcm_app.h"

namespace bcm {
namespace app {
namespace {

using services::LampDrive;
using services::LampId;

const uint16_t kCyclePeriodMs = 5U;
const uint16_t kHeartbeatMs   = 500U;

/// bsp::Lamp and services::LampId are kept numerically identical; assert it
/// rather than trusting a comment.
static_assert(static_cast<uint8_t>(bsp::Lamp::Count) ==
              static_cast<uint8_t>(LampId::Count),
              "bsp::Lamp and services::LampId have diverged");

}  // namespace

BcmApp::BcmApp()
    : cfg_(),
      lighting_(cfg_),
      door_(cfg_),
      power_(cfg_),
      horn_(cfg_),
      faults_(),
      buzzer_(),
      battery_(),
      clock_(),
      transport_(),
      dispatcher_(command_table(), command_table_size(), &cmd_ctx_),
      comm_(transport_, dispatcher_),
      cmd_ctx_(),
      host_request_(),
      actual_(),
      ignition_held_ms_(0U),
      long_press_fired_(false),
      heartbeat_ms_(0U),
      battery_permille_(0U),
      prev_brake_(false),
      prev_load_shed_(false),
      prev_comms_lost_(false),
      prev_door_(services::DoorState::Locked)
{
}

uint16_t BcmApp::sample_battery()
{
    battery_.sample();
    return battery_.permille();
}

bool BcmApp::take_log_event(services::LogEvent& event, uint8_t& arg)
{
    services::LogRecord r;
    if (!log_.pop(r)) { return false; }
    event = r.event;
    arg   = r.arg;
    return true;
}

void BcmApp::step(uint16_t dt_ms)
{
    read_inputs(dt_ms);
    feed_managers(dt_ms);
    publish(dt_ms);

    (void)comm_.poll(dt_ms);
    if (cmd_ctx_.reset_requested) {
        drivers::Tick::delay_ms(20U);   /* let the reply drain */
        NVIC_SystemReset();
    }
}

void BcmApp::init()
{
    for (uint8_t i = 0U; i < static_cast<uint8_t>(bsp::Lamp::Count); ++i) {
        lamps_[i] = drivers::Led(bsp::lamp_pin(static_cast<bsp::Lamp>(i)));
        lamps_[i].init();
    }
    for (uint8_t i = 0U; i < static_cast<uint8_t>(bsp::Switch::Count); ++i) {
        buttons_[i] = drivers::Button(bsp::switch_pin(static_cast<bsp::Switch>(i)));
        buttons_[i].init(bsp::switch_pull());
    }

    buzzer_ = bsp::buzzer_pin();
    buzzer_.config_output();
    buzzer_.write(false);

    battery_ = drivers::AnalogInput(bsp::adc_battery_channel());

    drivers::Uart::init(bsp::uart_config());
    drivers::Uart::write_str("\r\nBCM ready - BCM-ICD-001 v1.0\r\n");

    /* A watchdog reset is a fault worth recording, not a silent restart. */
    if (drivers::Watchdog::reset_was_watchdog()) {
        faults_.set(services::Dtc::WatchdogReset);
    }

    cmd_ctx_.host_request = &host_request_;
    cmd_ctx_.actual       = &actual_;
    cmd_ctx_.battery      = &battery_;
    cmd_ctx_.faults       = &faults_;
}

void BcmApp::read_inputs(uint16_t dt_ms)
{
    for (uint8_t i = 0U; i < static_cast<uint8_t>(bsp::Switch::Count); ++i) {
        buttons_[i].update(dt_ms);
    }
    /* The ADC conversion is NOT done here: it blocks for tens of
     * microseconds, and the control cycle must stay short. The Sensor task
     * performs it and injects the result via set_battery_permille(). */

    uint8_t bitmap = 0U;
    for (uint8_t i = 0U; i < static_cast<uint8_t>(bsp::Switch::Count); ++i) {
        if (buttons_[i].pressed()) { bitmap = static_cast<uint8_t>(bitmap | (1U << i)); }
    }
    cmd_ctx_.switch_bitmap = bitmap;
    cmd_ctx_.power_state   = static_cast<uint8_t>(power_.state());
}

void BcmApp::feed_managers(uint16_t dt_ms)
{
    drivers::Button& ign   = buttons_[static_cast<uint8_t>(bsp::Switch::Ignition)];
    drivers::Button& left  = buttons_[static_cast<uint8_t>(bsp::Switch::IndLeft)];
    drivers::Button& right = buttons_[static_cast<uint8_t>(bsp::Switch::IndRight)];
    drivers::Button& haz   = buttons_[static_cast<uint8_t>(bsp::Switch::Hazard)];
    drivers::Button& lock  = buttons_[static_cast<uint8_t>(bsp::Switch::DoorLock)];

    /* --- Ignition: short press toggles power, long press steps the lights.
     * The board has no dedicated light switch yet (see board_config.h). */
    if (ign.pressed()) {
        ignition_held_ms_ = static_cast<uint16_t>(ignition_held_ms_ + dt_ms);
        if (!long_press_fired_ && ignition_held_ms_ >= cfg_.long_press_ms) {
            long_press_fired_ = true;
            lighting_.next_mode();
        }
    }
    if (ign.just_released()) {
        if (!long_press_fired_) {
            if (power_.running()) { power_.ignition_off(); }
            else                  { power_.ignition_on(); }
        }
        ignition_held_ms_ = 0U;
        long_press_fired_ = false;
    }

    if (left.just_pressed()) {
        lighting_.indicator_left();
        log_.log(0U, services::LogEvent::IndicatorLeft);
    }
    if (right.just_pressed()) {
        lighting_.indicator_right();
        log_.log(0U, services::LogEvent::IndicatorRight);
    }
    if (haz.just_pressed()) {
        lighting_.indicator_hazard();
        log_.log(0U, (lighting_.indicator_state() == services::IndState::Hazard)
                         ? services::LogEvent::HazardOn
                         : services::LogEvent::HazardOff);
    }

    /* --- Doors: button toggles, and the host can request the same. */
    if (lock.just_pressed()) {
        if (door_.locked()) { door_.request_unlock(); }
        else                { door_.request_lock(); }
    }
    if (cmd_ctx_.door_lock_requested) {
        cmd_ctx_.door_lock_requested = false;
        door_.request_lock();
    }
    if (cmd_ctx_.door_unlock_requested) {
        cmd_ctx_.door_unlock_requested = false;
        door_.request_unlock();
    }

    /* A refused lock is a diagnosable event, not a silent no-op. */
    if (door_.take_lock_refused()) {
        faults_.set(services::Dtc::LockRefusedDoorOpen);
        log_.log(0U, services::LogEvent::LockRefused);
    }

    if (door_.take_chirp_lock())   { horn_.chirp(1U); }
    if (door_.take_chirp_unlock()) { horn_.chirp(2U); }

    power_.update_battery(battery_permille_);
    power_.update(dt_ms);
    door_.update(dt_ms);

    /* Battery and comms faults track their live condition. */
    if (power_.load_shed()) { faults_.set(services::Dtc::BatteryLow); }
    else                    { faults_.clear(services::Dtc::BatteryLow); }

    if (comm_.link_lost()) { faults_.set(services::Dtc::CommsLost); }
    else                   { faults_.clear(services::Dtc::CommsLost); }

    if (drivers::Uart::take_overrun()) { faults_.set(services::Dtc::UartOverrun); }

    /* ---- Log on change only. A 5 ms loop would otherwise flood the log. -- */
    const bool brake_now = buttons_[static_cast<uint8_t>(bsp::Switch::Brake)].pressed();
    if (brake_now != prev_brake_) {
        prev_brake_ = brake_now;
        log_.log(0U, brake_now ? services::LogEvent::BrakeApplied
                               : services::LogEvent::BrakeReleased);
    }

    if (power_.load_shed() != prev_load_shed_) {
        prev_load_shed_ = power_.load_shed();
        log_.log(0U, prev_load_shed_ ? services::LogEvent::BatteryLow
                                     : services::LogEvent::BatteryOk);
    }

    if (comm_.link_lost() != prev_comms_lost_) {
        prev_comms_lost_ = comm_.link_lost();
        log_.log(0U, prev_comms_lost_ ? services::LogEvent::CommsLost
                                      : services::LogEvent::CommsRestored);
    }

    if (door_.state() != prev_door_) {
        prev_door_ = door_.state();
        log_.log(0U, (prev_door_ == services::DoorState::Locked)
                         ? services::LogEvent::DoorLocked
                         : services::LogEvent::DoorUnlocked);
    }
}

void BcmApp::publish(uint16_t dt_ms)
{
    /* 1. Managers publish what they want lit. */
    services::LampState wanted;
    power_.apply(wanted);
    lighting_.apply(wanted);
    door_.apply(wanted);

    /* 2. Merge whatever the diagnostic host has asked for. */
    for (uint8_t i = 0U; i < static_cast<uint8_t>(LampId::Count); ++i) {
        if ((cmd_ctx_.host_mask & (1U << i)) != 0U) {
            wanted.drive[i] = host_request_.drive[i];
        }
    }

    /* 3. Arbitrate - the last word before the pins. */
    services::ArbiterInputs arb;
    arb.brake_pressed   = buttons_[static_cast<uint8_t>(bsp::Switch::Brake)].pressed();
    arb.reverse_gear    = false;   /* no gear switch wired yet */
    arb.lamps_permitted = power_.lamps_permitted();
    arb.load_shed       = power_.load_shed();
    arb.comms_lost      = comm_.link_lost();
    services::LampArbiter::arbitrate(wanted, arb);

    actual_ = wanted;

    /* 4. Drive the outputs. */
    for (uint8_t i = 0U; i < static_cast<uint8_t>(LampId::Count); ++i) {
        drivers::Led& led = lamps_[i];
        switch (wanted.drive[i]) {
        case LampDrive::On:    led.on(); break;
        case LampDrive::Blink: led.blink(cfg_.indicator_on_ms, cfg_.indicator_off_ms); break;
        case LampDrive::Off:
        default:               led.off(); break;
        }
        led.update(dt_ms);
    }

    buzzer_.write(horn_.update(dt_ms));
}

void BcmApp::run()
{
    for (;;) {
        const uint16_t dt = clock_.tick();

        /* Bare-metal: nobody else samples the ADC, so do it here. */
        battery_permille_ = sample_battery();
        step(dt);

        heartbeat_ms_ = static_cast<uint16_t>(heartbeat_ms_ + dt);
        if (heartbeat_ms_ >= kHeartbeatMs) {
            heartbeat_ms_ = 0U;
            bsp::heartbeat_toggle();
        }

        drivers::Tick::delay_ms(kCyclePeriodMs);
    }
}

}  // namespace app
}  // namespace bcm
