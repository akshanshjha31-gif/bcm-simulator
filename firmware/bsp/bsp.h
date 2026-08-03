/**
 * @file    bsp.h
 * @brief   Board Support Package - thin C++ facade over board wiring.
 *
 * The BSP is the only layer permitted to touch board_config.h pin macros. It
 * hands out configured GpioPin / UartConfig values; behaviour (debounce,
 * blink, filtering) belongs to the driver layer, not here.
 */
#ifndef BCM_BSP_H
#define BCM_BSP_H

#include "drv_gpio.h"
#include "drv_uart.h"
#include <stdint.h>

namespace bcm {
namespace bsp {

/// Lamp outputs, in physical order for the walk test.
enum class Lamp : uint8_t {
    Ignition = 0,   ///< PB0
    Drl,            ///< PB1
    LowBeam,        ///< PB10
    HighBeam,       ///< PB11
    IndLeft,        ///< PB12
    IndRight,       ///< PB13
    Hazard,         ///< PB14
    Brake,          ///< PB15
    Reverse,        ///< PA8
    DoorLock,       ///< PA9 - appended, so BCM-ICD-001 lamp ids stay stable
    Count
};

/// Discrete switch inputs.
enum class Switch : uint8_t {
    Ignition = 0,   ///< PA0
    IndLeft,        ///< PA1
    IndRight,       ///< PA2
    Hazard,         ///< PA3
    Brake,          ///< PA4
    DoorLock,       ///< PA5
    Count
};

/// Enable port clocks, configure the heartbeat LED and bring up the ADC.
void init();

/// Toggle the on-board heartbeat LED. Proves the system is alive.
void heartbeat_toggle();

/// Drive the heartbeat LED to an explicit state (true = visibly on).
void heartbeat_set(bool on);

/// Pin for a lamp, with board polarity already applied.
drivers::GpioPin lamp_pin(Lamp lamp);

/// Pin for a switch, with board polarity already applied.
drivers::GpioPin switch_pin(Switch sw);

/// Buzzer / horn pin.
drivers::GpioPin buzzer_pin();

/// Pull configuration the switches need (NOPULL when the board has external
/// pull resistors fitted).
uint32_t switch_pull();

/// Analog pins are configured by init(); these give the ADC channel numbers.
uint32_t adc_battery_channel();
uint32_t adc_ambient_channel();

/// Diagnostic UART wiring and baud rate.
drivers::UartConfig uart_config();

/// Expected core clock, so POST can verify the PLL actually locked. Exposed
/// through the BSP because board_config.h is off-limits to upper layers.
uint32_t sysclk_hz();

/// Human-readable lamp name, for diagnostics.
const char* lamp_name(Lamp lamp);

/// Number of lamps / switches, for iteration.
inline uint8_t lamp_count() { return static_cast<uint8_t>(Lamp::Count); }
inline uint8_t switch_count() { return static_cast<uint8_t>(Switch::Count); }

}  // namespace bsp
}  // namespace bcm

#endif /* BCM_BSP_H */
