/**
 * @file    bcm_types.h
 * @brief   Shared BCM domain types - pure logic, no HAL.
 *
 * Lamp and switch identities used by the service layer. Deliberately mirrors
 * bsp::Lamp / bsp::Switch numerically (and BCM-ICD-001 section 4.2), but is
 * declared here so services never include a board header.
 */
#ifndef BCM_TYPES_H
#define BCM_TYPES_H

#include <stdint.h>

namespace bcm {
namespace services {

/// Lamp identities. Values match BCM-ICD-001 lamp ids.
enum class LampId : uint8_t {
    Ignition = 0,
    Drl      = 1,
    LowBeam  = 2,
    HighBeam = 3,
    IndLeft  = 4,
    IndRight = 5,
    Hazard   = 6,
    Brake    = 7,
    Reverse  = 8,
    DoorLock = 9,
    Count    = 10
};

/// How a lamp should be driven this cycle.
enum class LampDrive : uint8_t {
    Off,
    On,
    Blink        ///< driver applies the indicator flash rate
};

/// Snapshot of every lamp for one cycle.
struct LampState {
    LampDrive drive[static_cast<uint8_t>(LampId::Count)];

    LampState()
    {
        for (uint8_t i = 0U; i < static_cast<uint8_t>(LampId::Count); ++i) {
            drive[i] = LampDrive::Off;
        }
    }

    LampDrive get(LampId id) const
    {
        return (id < LampId::Count) ? drive[static_cast<uint8_t>(id)]
                                    : LampDrive::Off;
    }

    void set(LampId id, LampDrive d)
    {
        if (id < LampId::Count) { drive[static_cast<uint8_t>(id)] = d; }
    }

    /// Bitmap of lamps that are not Off, for GET_STATUS.
    uint16_t bitmap() const
    {
        uint16_t m = 0U;
        for (uint8_t i = 0U; i < static_cast<uint8_t>(LampId::Count); ++i) {
            if (drive[i] != LampDrive::Off) {
                m = static_cast<uint16_t>(m | (1U << i));
            }
        }
        return m;
    }
};

/**
 * @brief Debounced inputs plus filtered analog values for one cycle.
 *
 * Produced by sensor_mgr, consumed by every manager. Passing a plain struct
 * rather than letting managers read pins keeps them all host-testable.
 */
struct Inputs {
    bool ignition;
    bool ind_left;
    bool ind_right;
    bool hazard;
    bool brake;
    bool door_lock;

    /* Not yet wired on this board; injected over the diagnostic link. */
    bool reverse_gear;
    bool door_open;

    uint16_t battery_permille;   ///< 0..1000 of full scale
    uint16_t ambient_permille;   ///< 0..1000, lower = darker

    Inputs()
        : ignition(false), ind_left(false), ind_right(false), hazard(false),
          brake(false), door_lock(false), reverse_gear(false),
          door_open(false), battery_permille(0U), ambient_permille(1000U)
    {
    }
};

/// Rising-edge flags derived from two consecutive Inputs snapshots.
struct Edges {
    bool ignition;
    bool ind_left;
    bool ind_right;
    bool hazard;
    bool door_lock;

    Edges()
        : ignition(false), ind_left(false), ind_right(false),
          hazard(false), door_lock(false)
    {
    }
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_TYPES_H */
