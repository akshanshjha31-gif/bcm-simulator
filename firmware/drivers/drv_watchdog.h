/**
 * @file    drv_watchdog.h
 * @brief   Independent watchdog (IWDG).
 *
 * The IWDG runs from the ~40 kHz LSI, so it keeps counting even if the main
 * clock tree fails. Once started it cannot be stopped - by design.
 *
 * In Phase 4 a supervisor task refreshes this only when every registered task
 * has checked in, turning a hung task into a controlled reset
 * (SRS-SAFETY-006).
 */
#ifndef BCM_DRV_WATCHDOG_H
#define BCM_DRV_WATCHDOG_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

namespace bcm {
namespace drivers {

class Watchdog {
public:
    /**
     * @brief Start the IWDG.
     * @param timeout_ms desired timeout; clamped to what the LSI can express
     *                   (roughly 1..26000 ms with prescaler 256).
     * @return false if the peripheral rejected the configuration.
     */
    static bool start(uint32_t timeout_ms);

    /// Kick the dog. Must be called more often than the timeout.
    static void refresh();

    /// True if the last reset was caused by the watchdog rather than power-on.
    /// Reads and clears the RCC reset flags, so call once at startup.
    static bool reset_was_watchdog();

    static bool running() { return running_; }

private:
    static IWDG_HandleTypeDef handle_;
    static bool               running_;
};

}  // namespace drivers
}  // namespace bcm

#endif /* BCM_DRV_WATCHDOG_H */
