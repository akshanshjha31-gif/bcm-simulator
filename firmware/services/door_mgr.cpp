/**
 * @file    door_mgr.cpp
 * @brief   Door state machine.
 */
#include "door_mgr.h"

namespace bcm {
namespace services {
namespace {

/**
 * @brief SRS-DOOR-002 / SRS-SAFETY-004 - never lock with a door open.
 *
 * Expressed once, as a guard on every locking transition. Unlocking has no
 * such guard by design: refusing to unlock could trap an occupant.
 */
bool no_door_open(const DoorContext& c) { return !c.door_open; }

void on_locked(DoorContext& c)
{
    c.chirp_lock   = true;
    c.lock_refused = false;
}

void on_unlocked(DoorContext& c)
{
    c.chirp_unlock = true;
    c.lock_refused = false;
}

typedef FsmTransition<DoorState, DoorEvent, DoorContext> DoorRow;

const DoorRow kTable[] = {
    /* from                 any    event                       to                    guard          action */
    { DoorState::Unlocked,  false, DoorEvent::LockRequest,     DoorState::Locked,   no_door_open,  on_locked   },
    { DoorState::Welcome,   false, DoorEvent::LockRequest,     DoorState::Locked,   no_door_open,  on_locked   },

    { DoorState::Locked,    false, DoorEvent::UnlockRequest,   DoorState::Welcome,  0,             on_unlocked },

    { DoorState::Welcome,   false, DoorEvent::WelcomeExpired,  DoorState::Unlocked, 0,             0 },

    /* Auto-lock carries the same guard - an open door blocks it too. */
    { DoorState::Unlocked,  false, DoorEvent::AutoLockElapsed, DoorState::Locked,   no_door_open,  on_locked   },
};

const uint8_t kRows = sizeof(kTable) / sizeof(kTable[0]);

}  // namespace

DoorMgr::DoorMgr(const Config& config)
    : cfg_(config),
      fsm_(kTable, kRows, DoorState::Locked),
      ctx_(),
      welcome_(),
      auto_lock_()
{
}

void DoorMgr::request_lock()
{
    const uint32_t before = fsm_.blocked_count();

    if (!fsm_.dispatch(DoorEvent::LockRequest, ctx_)) {
        /* Distinguish "guard refused" from "no rule for this state", so the
         * driver gets told why rather than seeing silence. */
        if (fsm_.blocked_count() != before) { ctx_.lock_refused = true; }
        return;
    }
    welcome_.stop();
    auto_lock_.stop();
}

void DoorMgr::request_unlock()
{
    if (fsm_.dispatch(DoorEvent::UnlockRequest, ctx_)) {
        welcome_.start(cfg_.welcome_duration_ms);
        auto_lock_.stop();
    }
}

void DoorMgr::update(uint32_t dt_ms)
{
    if (welcome_.update(dt_ms)) {
        fsm_.dispatch(DoorEvent::WelcomeExpired, ctx_);
        /* Unlocked and idle: start counting toward auto-lock. */
        auto_lock_.start(cfg_.auto_lock_delay_ms);
    }

    if (auto_lock_.update(dt_ms)) {
        if (!fsm_.dispatch(DoorEvent::AutoLockElapsed, ctx_)) {
            /* Refused because a door is open. Re-arm and try again later
             * rather than silently giving up for good. */
            auto_lock_.start(cfg_.auto_lock_delay_ms);
        }
    }
}

void DoorMgr::apply(LampState& out) const
{
    /* The lock lamp shows locked state; welcome lighting also lifts the DRL
     * so there is visible light around the car (SRS-DOOR-004). */
    if (fsm_.state() == DoorState::Locked) {
        out.set(LampId::DoorLock, LampDrive::On);
    }
    if (fsm_.state() == DoorState::Welcome) {
        out.set(LampId::Drl, LampDrive::On);
    }
}

bool DoorMgr::take_lock_refused()
{
    const bool was = ctx_.lock_refused;
    ctx_.lock_refused = false;
    return was;
}

bool DoorMgr::take_chirp_lock()
{
    const bool was = ctx_.chirp_lock;
    ctx_.chirp_lock = false;
    return was;
}

bool DoorMgr::take_chirp_unlock()
{
    const bool was = ctx_.chirp_unlock;
    ctx_.chirp_unlock = false;
    return was;
}

void DoorMgr::reset()
{
    fsm_.set_state(DoorState::Locked);
    ctx_ = DoorContext();
    welcome_.stop();
    auto_lock_.stop();
}

}  // namespace services
}  // namespace bcm
