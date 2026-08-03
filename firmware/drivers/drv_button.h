/**
 * @file    drv_button.h
 * @brief   Debounced switch input with edge detection.
 *
 * Composes a GpioPin with a Debouncer. The debounce logic is HAL-free and
 * unit-tested; this class only binds it to a pin.
 */
#ifndef BCM_DRV_BUTTON_H
#define BCM_DRV_BUTTON_H

#include "drv_gpio.h"
#include "debounce.h"

namespace bcm {
namespace drivers {

class Button {
public:
    Button() : debouncer_(kDefaultDebounceMs) {}

    explicit Button(const GpioPin& pin, uint16_t debounce_ms = kDefaultDebounceMs)
        : pin_(pin), debouncer_(debounce_ms)
    {
    }

    /// Configure the pin. `pull` is GPIO_NOPULL when the board has an
    /// external pull resistor, otherwise GPIO_PULLUP / GPIO_PULLDOWN.
    void init(uint32_t pull)
    {
        pin_.config_input(pull);
        debouncer_.reset(pin_.read());
    }

    /// Sample and debounce. Call every loop pass.
    void update(uint16_t dt_ms) { debouncer_.update(pin_.read(), dt_ms); }

    /// Debounced level - true while held down.
    bool pressed() const { return debouncer_.state(); }

    /// True only on the update() that saw the press begin.
    bool just_pressed() const { return debouncer_.rising(); }

    /// True only on the update() that saw the release.
    bool just_released() const { return debouncer_.falling(); }

private:
    /// 20 ms covers typical contact bounce and leaves headroom inside the
    /// 50 ms switch-to-lamp budget (SRS-PERF-001).
    static const uint16_t kDefaultDebounceMs = 20U;

    GpioPin   pin_;
    Debouncer debouncer_;
};

}  // namespace drivers
}  // namespace bcm

#endif /* BCM_DRV_BUTTON_H */
