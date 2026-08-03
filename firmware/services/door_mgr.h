/**
 * @file    door_mgr.h
 * @brief   Door state machine - pure logic, no HAL.
 *
 * Locked -> Unlocked -> Welcome -> (Auto-Lock) -> Locked
 *
 * The load-bearing rule is SRS-DOOR-002: **the BCM shall not lock while any
 * door is open.** That is expressed as an FSM guard rather than an `if` at
 * each call site, so no future caller can route around it.
 *
 * Traces SRS-DOOR-001..004, SRS-SAFETY-004.
 */
#ifndef BCM_DOOR_MGR_H
#define BCM_DOOR_MGR_H

#include "bcm_config.h"
#include "bcm_types.h"
#include "fsm.h"
#include "soft_timer.h"

namespace bcm {
namespace services {

enum class DoorState : uint8_t {
    Locked = 0,
    Unlocked,
    Welcome        ///< briefly lit after unlocking (SRS-DOOR-004)
};

enum class DoorEvent : uint8_t {
    LockRequest,
    UnlockRequest,
    WelcomeExpired,
    AutoLockElapsed
};

struct DoorContext {
    bool door_open;        ///< any door ajar - blocks locking
    bool lock_refused;     ///< set when a guard rejected a lock request
    bool chirp_lock;       ///< one-shot: horn should chirp for lock
    bool chirp_unlock;     ///< one-shot: horn should chirp for unlock

    DoorContext()
        : door_open(false), lock_refused(false),
          chirp_lock(false), chirp_unlock(false)
    {
    }
};

class DoorMgr {
public:
    explicit DoorMgr(const Config& config);

    /// Tell the manager whether any door is physically open.
    void set_door_open(bool open) { ctx_.door_open = open; }

    /// Driver pressed lock. Refused (and flagged) if a door is open.
    void request_lock();

    /// Driver pressed unlock. Always permitted - never trap anyone in.
    void request_unlock();

    /// Advance welcome / auto-lock timers.
    void update(uint32_t dt_ms);

    void apply(LampState& out) const;

    DoorState state() const { return fsm_.state(); }
    bool      locked() const { return fsm_.state() == DoorState::Locked; }

    /// True if the most recent lock request was refused by the open-door
    /// guard. Cleared by reading, and reported over the diagnostic link.
    bool take_lock_refused();

    /// One-shot horn requests, consumed by horn_mgr.
    bool take_chirp_lock();
    bool take_chirp_unlock();

    void reset();

private:
    const Config&                                 cfg_;
    Fsm<DoorState, DoorEvent, DoorContext>        fsm_;
    DoorContext                                   ctx_;
    drivers::SoftTimer                            welcome_;
    drivers::SoftTimer                            auto_lock_;
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_DOOR_MGR_H */
