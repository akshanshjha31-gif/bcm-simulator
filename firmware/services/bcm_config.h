/**
 * @file    bcm_config.h
 * @brief   Central tunables (config_mgr) - pure logic, no HAL.
 *
 * Every timing and threshold in the BCM lives here rather than as a literal
 * buried in a manager, so behaviour can be retuned in one place and the
 * coding standard's "no magic numbers" rule is actually enforceable.
 *
 * Phase 3 holds these as compile-time defaults in a single struct; flash
 * persistence arrives with the full config_mgr.
 */
#ifndef BCM_CONFIG_H
#define BCM_CONFIG_H

#include <stdint.h>

namespace bcm {
namespace services {

struct Config {
    /* ---- Indicators (SRS-IND-002) --------------------------------------- */
    uint16_t indicator_on_ms;
    uint16_t indicator_off_ms;

    /* ---- Auto-headlight (SRS-LIGHT-003) --------------------------------- */
    uint16_t ambient_dark_permille;    ///< below this, switch low beam on
    uint16_t ambient_light_permille;   ///< above this, switch it off again

    /* ---- Doors (SRS-DOOR-003/004) --------------------------------------- */
    uint32_t auto_lock_delay_ms;
    uint32_t welcome_duration_ms;

    /* ---- Horn (SRS-HORN-002) -------------------------------------------- */
    uint16_t chirp_ms;
    uint16_t chirp_gap_ms;
    uint32_t horn_max_on_ms;           ///< rate limit, protects the buzzer

    /* ---- Power / battery (SRS-PWR-004) ---------------------------------- */
    uint16_t battery_low_permille;     ///< below this, shed non-essential load
    uint16_t battery_ok_permille;      ///< recovery threshold (hysteresis)
    uint32_t sleep_after_ms;           ///< inactivity before entering Sleep

    /* ---- Long-press on the ignition button ------------------------------ */
    uint16_t long_press_ms;

    /// Defaults chosen to be visibly correct on the bench.
    Config()
        : indicator_on_ms(333U),
          indicator_off_ms(333U),
          ambient_dark_permille(300U),
          ambient_light_permille(400U),
          auto_lock_delay_ms(3000U),
          welcome_duration_ms(2000U),
          chirp_ms(80U),
          chirp_gap_ms(120U),
          horn_max_on_ms(10000U),
          battery_low_permille(300U),
          battery_ok_permille(350U),
          sleep_after_ms(30000U),
          long_press_ms(800U)
    {
    }
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_CONFIG_H */
