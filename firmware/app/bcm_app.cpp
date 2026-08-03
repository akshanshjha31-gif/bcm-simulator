/**
 * @file    bcm_app.cpp
 * @brief   Application composition root.
 *
 * Deliberately thin. All BCM behaviour lives in services::BcmCore, which is
 * hardware-free; this class only converts pins into an Inputs snapshot, hands
 * it over, and drives the arbitrated result back onto the pins. That is what
 * lets the SIL harness exercise the shipped logic rather than a copy of it.
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
      core_(cfg_),
      buzzer_(),
      battery_(),
      clock_(),
      transport_(),
      dispatcher_(command_table(), command_table_size(), &cmd_ctx_),
      comm_(transport_, dispatcher_),
      cmd_ctx_(),
      host_request_(),
      actual_(),
      heartbeat_ms_(0U),
      battery_permille_(0U)
{
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

    /* No text banner: the link carries protocol frames only, so a host never
     * has to resynchronise around human-readable output. Startup is announced
     * as a LOG_EVENT frame instead. */
    drivers::Uart::init(bsp::uart_config());

    /* A watchdog reset is a fault worth recording, not a silent restart. */
    if (drivers::Watchdog::reset_was_watchdog()) {
        core_.faults().set(services::Dtc::WatchdogReset);
    }

    cmd_ctx_.host_request = &host_request_;
    cmd_ctx_.actual       = &actual_;
    cmd_ctx_.battery      = &battery_;
    cmd_ctx_.faults       = &core_.faults();
}

uint16_t BcmApp::sample_battery()
{
    battery_.sample();
    return battery_.permille();
}

bool BcmApp::take_log_event(services::LogEvent& event, uint8_t& arg)
{
    services::LogRecord r;
    if (!core_.log().pop(r)) { return false; }
    event = r.event;
    arg   = r.arg;
    return true;
}

void BcmApp::gather_inputs(uint16_t dt_ms)
{
    for (uint8_t i = 0U; i < static_cast<uint8_t>(bsp::Switch::Count); ++i) {
        buttons_[i].update(dt_ms);
    }
    /* The ADC conversion is NOT done here: it blocks for tens of
     * microseconds, and the control cycle must stay short. The Sensor task
     * performs it and injects the result via set_battery_permille(). */

    services::Inputs in;
    in.ignition  = buttons_[static_cast<uint8_t>(bsp::Switch::Ignition)].pressed();
    in.ind_left  = buttons_[static_cast<uint8_t>(bsp::Switch::IndLeft)].pressed();
    in.ind_right = buttons_[static_cast<uint8_t>(bsp::Switch::IndRight)].pressed();
    in.hazard    = buttons_[static_cast<uint8_t>(bsp::Switch::Hazard)].pressed();
    in.brake     = buttons_[static_cast<uint8_t>(bsp::Switch::Brake)].pressed();
    in.door_lock = buttons_[static_cast<uint8_t>(bsp::Switch::DoorLock)].pressed();

    /* Not wired on this board yet - pins are reserved behind BCM_HW_EXT_IO.
     * Until then the diagnostic link is the only way to assert them. */
    in.reverse_gear = false;
    in.door_open    = false;

    in.battery_permille = battery_permille_;
    /* No LDR fitted, so report full daylight rather than a floating pin;
     * otherwise auto-headlight would latch on ADC noise. */
    in.ambient_permille = 1000U;

    core_.set_inputs(in);
    core_.set_comms_lost(comm_.link_lost());

    /* Expose the switch bitmap and power state for GET_STATUS. */
    uint8_t bitmap = 0U;
    for (uint8_t i = 0U; i < static_cast<uint8_t>(bsp::Switch::Count); ++i) {
        if (buttons_[i].pressed()) { bitmap = static_cast<uint8_t>(bitmap | (1U << i)); }
    }
    cmd_ctx_.switch_bitmap = bitmap;
    cmd_ctx_.power_state   = static_cast<uint8_t>(core_.power_state());

    /* Forward anything the diagnostic host asked for. */
    core_.set_host_request(host_request_, cmd_ctx_.host_mask);
    if (cmd_ctx_.door_lock_requested) {
        cmd_ctx_.door_lock_requested = false;
        core_.request_door_lock();
    }
    if (cmd_ctx_.door_unlock_requested) {
        cmd_ctx_.door_unlock_requested = false;
        core_.request_door_unlock();
    }

    if (drivers::Uart::take_overrun()) {
        core_.faults().set(services::Dtc::UartOverrun);
    }
}

void BcmApp::drive_outputs(uint16_t dt_ms)
{
    const services::LampState& wanted = core_.lamps();
    actual_ = wanted;

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

    buzzer_.write(core_.horn());
}

void BcmApp::step(uint16_t dt_ms)
{
    gather_inputs(dt_ms);
    core_.step(dt_ms);
    drive_outputs(dt_ms);

    (void)comm_.poll(dt_ms);
    if (cmd_ctx_.reset_requested) {
        drivers::Tick::delay_ms(20U);   /* let the reply drain */
        NVIC_SystemReset();
    }
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
