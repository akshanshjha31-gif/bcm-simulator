/**
 * @file    drv_gpio.h
 * @brief   Typed digital I/O over the STM32 HAL.
 *
 * Wraps a (port, pin, polarity) triple so everything above the driver layer
 * works in logical terms - "asserted" rather than "high". Active-low wiring
 * is handled here exactly once instead of being re-derived at every call site.
 */
#ifndef BCM_DRV_GPIO_H
#define BCM_DRV_GPIO_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

namespace bcm {
namespace drivers {

class GpioPin {
public:
    GpioPin() : port_(0), pin_(0U), active_low_(false) {}

    GpioPin(GPIO_TypeDef* port, uint16_t pin, bool active_low = false)
        : port_(port), pin_(pin), active_low_(active_low)
    {
    }

    /// Configure as push-pull output and drive it de-asserted.
    void config_output() const;

    /// Configure as digital input. `pull` is GPIO_NOPULL/PULLUP/PULLDOWN.
    void config_input(uint32_t pull) const;

    /// Configure for the ADC.
    void config_analog() const;

    /// Drive the pin. `asserted` is logical, polarity applied here.
    void write(bool asserted) const;

    /// Read the pin logically (true == asserted, whatever the polarity).
    bool read() const;

    void toggle() const;

    bool valid() const { return port_ != 0; }

private:
    GPIO_TypeDef* port_;
    uint16_t      pin_;
    bool          active_low_;
};

}  // namespace drivers
}  // namespace bcm

#endif /* BCM_DRV_GPIO_H */
