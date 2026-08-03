/**
 * @file    drv_timer.h
 * @brief   Millisecond tick source and wrap-safe delta clock.
 *
 * Everything above this layer is driven by *elapsed* milliseconds rather than
 * absolute ticks, which is what lets the same logic run in unit tests and the
 * SIL where no HAL tick exists.
 */
#ifndef BCM_DRV_TIMER_H
#define BCM_DRV_TIMER_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

namespace bcm {
namespace drivers {

class Tick {
public:
    /// Free-running millisecond counter (HAL SysTick).
    static uint32_t now_ms() { return HAL_GetTick(); }

    /// Busy-wait. Only for bring-up - never on a real-time path.
    static void delay_ms(uint32_t ms) { HAL_Delay(ms); }
};

/**
 * @brief Turns the absolute tick into a per-pass delta.
 *
 * Unsigned subtraction makes the 49.7-day counter wrap harmless. The result
 * is saturated to 16 bits because every consumer takes uint16_t dt.
 */
class DeltaClock {
public:
    DeltaClock() : last_(Tick::now_ms()) {}

    uint16_t tick()
    {
        const uint32_t now   = Tick::now_ms();
        const uint32_t delta = now - last_;   /* wrap-safe */
        last_ = now;
        return (delta > 0xFFFFU) ? 0xFFFFU : static_cast<uint16_t>(delta);
    }

    void reset() { last_ = Tick::now_ms(); }

private:
    uint32_t last_;
};

}  // namespace drivers
}  // namespace bcm

#endif /* BCM_DRV_TIMER_H */
