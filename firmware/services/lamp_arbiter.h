/**
 * @file    lamp_arbiter.h
 * @brief   Central lamp arbitration and safety invariants - pure logic.
 *
 * Every manager publishes what it *wants* lit. This is the single place that
 * decides what actually lights, and it is the last thing to touch the lamp
 * state before the drivers do. Centralising it is the whole point: safety
 * rules that are scattered across feature code get violated the moment
 * someone adds a feature (architecture document, driver 3).
 *
 * Rules applied, in order:
 *   1. Sleep blanks every lamp except those explicitly permitted (SRS-PWR-003)
 *   2. Load shedding removes non-essential lamps            (SRS-PWR-004)
 *   3. Reverse lamp only with reverse gear selected         (SRS-REV-001, SAFETY-005)
 *   4. Brake lamp overrides everything on its output        (SRS-BRK-001, SAFETY-003)
 *   5. Comms lost drives a defined safe state               (SRS-SAFETY-007)
 *
 * Brake is applied last precisely because it must win.
 */
#ifndef BCM_LAMP_ARBITER_H
#define BCM_LAMP_ARBITER_H

#include "bcm_types.h"

namespace bcm {
namespace services {

/// Everything arbitration needs to know that is not already in LampState.
struct ArbiterInputs {
    bool brake_pressed;
    bool reverse_gear;
    bool lamps_permitted;   ///< false in Sleep
    bool load_shed;         ///< battery low
    bool comms_lost;

    ArbiterInputs()
        : brake_pressed(false), reverse_gear(false), lamps_permitted(true),
          load_shed(false), comms_lost(false)
    {
    }
};

class LampArbiter {
public:
    /**
     * @brief Apply every safety rule to @p lamps in place.
     *
     * Idempotent: arbitrating an already-arbitrated state changes nothing,
     * which matters because it runs every cycle.
     */
    static void arbitrate(LampState& lamps, const ArbiterInputs& in);

    /// True if @p id may stay lit while shedding load.
    static bool is_essential(LampId id);

private:
    static void blank_all(LampState& lamps);
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_LAMP_ARBITER_H */
