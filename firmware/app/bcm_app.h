/**
 * @file    bcm_app.h
 * @brief   Application composition root - wires drivers, services and BSP.
 *
 * This is the only place that knows about every layer at once. Managers stay
 * hardware-free and testable; this class binds them to real pins and runs the
 * cycle:
 *
 *     sample inputs -> feed managers -> collect lamp requests
 *                   -> merge host requests -> ARBITRATE -> drive outputs
 *
 * Arbitration is deliberately the last step before the pins, so no feature
 * and no diagnostic host can route around a safety rule.
 */
#ifndef BCM_APP_H
#define BCM_APP_H

#include "bcm_config.h"
#include "bcm_types.h"
#include "bsp.h"
#include "commands.h"
#include "comm_mgr.h"
#include "door_mgr.h"
#include "drv_adc.h"
#include "drv_button.h"
#include "drv_led.h"
#include "drv_timer.h"
#include "drv_watchdog.h"
#include "fault_mgr.h"
#include "horn_mgr.h"
#include "lamp_arbiter.h"
#include "lighting_mgr.h"
#include "logger.h"
#include "power_mgr.h"
#include "uart_transport.h"

namespace bcm {
namespace app {

class BcmApp {
public:
    BcmApp();

    /// Configure hardware and bring the services up.
    void init();

    /// Bare-metal super-loop. Never returns. Unused once FreeRTOS runs.
    void run();

    /// One control cycle. Called by the RTOS Control task.
    void step(uint16_t dt_ms);

    /// Perform an ADC conversion and return the filtered battery level in
    /// per mille. Called by the RTOS Sensor task, off the control path.
    uint16_t sample_battery();

    /// Inject the value the Sensor task most recently measured.
    void set_battery_permille(uint16_t permille) { battery_permille_ = permille; }

    /// Drain one queued event for the Logger task. False when empty.
    bool take_log_event(services::LogEvent& event, uint8_t& arg);

private:
    void read_inputs(uint16_t dt_ms);
    void feed_managers(uint16_t dt_ms);
    void publish(uint16_t dt_ms);

    services::Config      cfg_;
    services::LightingMgr lighting_;
    services::DoorMgr     door_;
    services::PowerMgr    power_;
    services::HornMgr     horn_;
    services::FaultMgr    faults_;
    services::Logger      log_;

    drivers::Led          lamps_[static_cast<uint8_t>(bsp::Lamp::Count)];
    drivers::Button       buttons_[static_cast<uint8_t>(bsp::Switch::Count)];
    drivers::GpioPin      buzzer_;
    drivers::AnalogInput  battery_;
    drivers::DeltaClock   clock_;

    UartTransport         transport_;
    services::Dispatcher  dispatcher_;
    services::CommMgr     comm_;
    CommandContext        cmd_ctx_;

    services::LampState   host_request_;
    services::LampState   actual_;

    /// Held long enough, the ignition button steps the light switch. A
    /// bring-up affordance until a dedicated switch is wired to PB5.
    uint16_t              ignition_held_ms_;
    bool                  long_press_fired_;

    uint16_t              heartbeat_ms_;

    /// Latest reading from the Sensor task (or from run() when bare-metal).
    uint16_t              battery_permille_;

    /* Previous values, so events are logged on change rather than every
     * cycle - a 5 ms loop would otherwise flood the log instantly. */
    bool                  prev_brake_;
    bool                  prev_load_shed_;
    bool                  prev_comms_lost_;
    services::DoorState   prev_door_;
};

}  // namespace app
}  // namespace bcm

#endif /* BCM_APP_H */
