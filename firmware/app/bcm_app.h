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
#include "bcm_core.h"
#include "bcm_types.h"
#include "bsp.h"
#include "commands.h"
#include "comm_mgr.h"
#include "drv_adc.h"
#include "drv_button.h"
#include "drv_led.h"
#include "drv_timer.h"
#include "drv_watchdog.h"
#include "logger.h"
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
    /// Read the pins into a hardware-free Inputs snapshot for the core.
    void gather_inputs(uint16_t dt_ms);
    /// Drive the pins from the core's arbitrated lamp state.
    void drive_outputs(uint16_t dt_ms);

    /* All BCM behaviour lives in BcmCore, which is hardware-free and is the
     * exact object the SIL harness drives. This class only binds it to pins. */
    services::Config      cfg_;
    services::BcmCore     core_;

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

    /// Reserved for the dedicated light switch once one is wired to PB5.
    uint16_t              heartbeat_ms_;

    /// Latest reading from the Sensor task (or from run() when bare-metal).
    uint16_t              battery_permille_;
};

}  // namespace app
}  // namespace bcm

#endif /* BCM_APP_H */
