/**
 * @file    lamp_arbiter.cpp
 * @brief   Central lamp arbitration and safety invariants.
 */
#include "lamp_arbiter.h"

namespace bcm {
namespace services {

bool LampArbiter::is_essential(LampId id)
{
    /* Safety lamps and the ones another road user relies on seeing. Comfort
     * and convenience lighting is what gets shed. */
    switch (id) {
    case LampId::Brake:
    case LampId::IndLeft:
    case LampId::IndRight:
    case LampId::Hazard:
    case LampId::LowBeam:
    case LampId::Reverse:
        return true;
    default:
        return false;
    }
}

void LampArbiter::blank_all(LampState& lamps)
{
    for (uint8_t i = 0U; i < static_cast<uint8_t>(LampId::Count); ++i) {
        lamps.drive[i] = LampDrive::Off;
    }
}

void LampArbiter::arbitrate(LampState& lamps, const ArbiterInputs& in)
{
    /* --- 1. Sleep: everything off except the lock indicator, which shows
     *        the vehicle is secured and must survive sleep (SRS-PWR-003). */
    if (!in.lamps_permitted) {
        const LampDrive lock = lamps.get(LampId::DoorLock);
        blank_all(lamps);
        lamps.set(LampId::DoorLock, lock);
    }

    /* --- 2. Comms lost: fall back to a DEFINED state rather than holding
     *        whatever the host last commanded (SRS-SAFETY-007). Hazards are
     *        left alone - a stranded vehicle should stay conspicuous. */
    if (in.comms_lost) {
        lamps.set(LampId::HighBeam, LampDrive::Off);
        lamps.set(LampId::Reverse,  LampDrive::Off);
    }

    /* --- 3. Load shedding: drop non-essential lamps (SRS-PWR-004). */
    if (in.load_shed) {
        for (uint8_t i = 0U; i < static_cast<uint8_t>(LampId::Count); ++i) {
            const LampId id = static_cast<LampId>(i);
            if (!is_essential(id)) { lamps.set(id, LampDrive::Off); }
        }
    }

    /* --- 4. Reverse lamp is gated on the gear, unconditionally
     *        (SRS-REV-001, SRS-SAFETY-005). Applied after shedding so it can
     *        never be re-enabled by a later rule. */
    lamps.set(LampId::Reverse,
              in.reverse_gear ? lamps.get(LampId::Reverse) : LampDrive::Off);

    /* --- 5. Brake has the highest priority of any lamp and overrides
     *        whatever else wanted that output (SRS-BRK-001, SAFETY-003).
     *        Deliberately last: nothing downstream may undo it, including
     *        load shedding above. */
    if (in.brake_pressed) {
        lamps.set(LampId::Brake, LampDrive::On);
    }
    else {
        lamps.set(LampId::Brake, LampDrive::Off);
    }
}

}  // namespace services
}  // namespace bcm
