/**
 * @file    drv_led.h
 * @brief   Lamp abstraction: on / off / blink, driven without blocking.
 *
 * Composes a GpioPin with a Blinker, so the flashing logic stays HAL-free and
 * unit-tested while this class only binds it to hardware.
 */
#ifndef BCM_DRV_LED_H
#define BCM_DRV_LED_H

#include "drv_gpio.h"
#include "blink.h"

namespace bcm {
namespace drivers {

class Led {
public:
    Led() {}

    explicit Led(const GpioPin& pin) : pin_(pin) {}

    /// Configure the pin as an output; lamp starts off.
    void init()
    {
        pin_.config_output();
        blinker_.set_off();
        pin_.write(false);
    }

    void on()
    {
        blinker_.set_on();
        pin_.write(true);
    }

    void off()
    {
        blinker_.set_off();
        pin_.write(false);
    }

    void set(bool state) { state ? on() : off(); }

    /// Flash. Starts lit, per SRS-IND-004.
    void blink(uint16_t on_ms, uint16_t off_ms)
    {
        blinker_.set_blink(on_ms, off_ms, true);
        pin_.write(blinker_.output());
    }

    /// Advance the pattern and drive the pin. Call every loop pass.
    void update(uint16_t dt_ms)
    {
        pin_.write(blinker_.update(dt_ms));
    }

    bool is_on() const { return blinker_.output(); }

    Blinker::Mode mode() const { return blinker_.mode(); }

private:
    GpioPin pin_;
    Blinker blinker_;
};

}  // namespace drivers
}  // namespace bcm

#endif /* BCM_DRV_LED_H */
