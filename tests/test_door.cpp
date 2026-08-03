/**
 * @file    test_door.cpp
 * @brief   Unit tests for the Door state machine.
 *
 * The central case is SRS-DOOR-002 / SRS-SAFETY-004: the BCM shall not lock
 * while any door is open. That rule is tested from every angle here, because
 * it is the one that must never regress.
 */
#include "catch2/catch.hpp"
#include "door_mgr.h"

using bcm::services::Config;
using bcm::services::DoorMgr;
using bcm::services::DoorState;
using bcm::services::LampDrive;
using bcm::services::LampId;
using bcm::services::LampState;

TEST_CASE("Doors start locked", "[door]")
{
    Config cfg;
    DoorMgr d(cfg);
    REQUIRE(d.state() == DoorState::Locked);
    REQUIRE(d.locked());
}

TEST_CASE("Unlocking runs the welcome phase then settles", "[door]")
{
    /* SRS-DOOR-004. */
    Config cfg;
    DoorMgr d(cfg);

    d.request_unlock();
    REQUIRE(d.state() == DoorState::Welcome);

    d.update(cfg.welcome_duration_ms);
    REQUIRE(d.state() == DoorState::Unlocked);
}

TEST_CASE("Welcome lighting is visible while it runs", "[door]")
{
    Config cfg;
    DoorMgr d(cfg);
    d.request_unlock();

    LampState l;
    d.apply(l);
    REQUIRE(l.get(LampId::Drl) == LampDrive::On);
}

TEST_CASE("The lock lamp shows the locked state", "[door]")
{
    Config cfg;
    DoorMgr d(cfg);

    LampState locked;
    d.apply(locked);
    REQUIRE(locked.get(LampId::DoorLock) == LampDrive::On);

    d.request_unlock();
    LampState unlocked;
    d.apply(unlocked);
    REQUIRE(unlocked.get(LampId::DoorLock) == LampDrive::Off);
}

TEST_CASE("Locking with a door open is REFUSED", "[door][safety]")
{
    /* SRS-DOOR-002 - the invariant this whole module exists to protect. */
    Config cfg;
    DoorMgr d(cfg);

    d.request_unlock();
    d.update(cfg.welcome_duration_ms);
    REQUIRE(d.state() == DoorState::Unlocked);

    d.set_door_open(true);
    d.request_lock();

    REQUIRE(d.state() == DoorState::Unlocked);   // must NOT have locked
    REQUIRE(d.take_lock_refused());              // and it says why
}

TEST_CASE("Locking succeeds once the door is closed", "[door][safety]")
{
    Config cfg;
    DoorMgr d(cfg);
    d.request_unlock();
    d.update(cfg.welcome_duration_ms);

    d.set_door_open(true);
    d.request_lock();
    REQUIRE(d.state() == DoorState::Unlocked);

    d.set_door_open(false);
    d.request_lock();
    REQUIRE(d.state() == DoorState::Locked);
}

TEST_CASE("Auto-lock is also blocked by an open door", "[door][safety]")
{
    /* The guard must cover the automatic path too, not just the button - a
     * rule enforced only on the manual route is not enforced at all. */
    Config cfg;
    DoorMgr d(cfg);

    d.request_unlock();
    d.update(cfg.welcome_duration_ms);
    d.set_door_open(true);

    for (uint32_t t = 0U; t < cfg.auto_lock_delay_ms * 3U; t += 100U) {
        d.update(100U);
    }
    REQUIRE(d.state() == DoorState::Unlocked);
}

TEST_CASE("Auto-lock engages after the delay when doors are shut", "[door]")
{
    /* SRS-DOOR-003. */
    Config cfg;
    DoorMgr d(cfg);

    d.request_unlock();
    d.update(cfg.welcome_duration_ms);
    REQUIRE(d.state() == DoorState::Unlocked);

    for (uint32_t t = 0U; t < cfg.auto_lock_delay_ms; t += 100U) {
        d.update(100U);
    }
    REQUIRE(d.state() == DoorState::Locked);
}

TEST_CASE("Auto-lock retries once the door is finally closed", "[door][safety]")
{
    /* A blocked auto-lock must re-arm, not give up permanently - otherwise
     * closing the door later leaves the car unlocked forever. */
    Config cfg;
    DoorMgr d(cfg);

    d.request_unlock();
    d.update(cfg.welcome_duration_ms);
    d.set_door_open(true);

    for (uint32_t t = 0U; t < cfg.auto_lock_delay_ms * 2U; t += 100U) {
        d.update(100U);
    }
    REQUIRE(d.state() == DoorState::Unlocked);

    d.set_door_open(false);
    for (uint32_t t = 0U; t < cfg.auto_lock_delay_ms * 2U; t += 100U) {
        d.update(100U);
    }
    REQUIRE(d.state() == DoorState::Locked);
}

TEST_CASE("Unlocking is never refused, even with a door open", "[door][safety]")
{
    /* Refusing to unlock could trap an occupant, so there is deliberately no
     * guard on that transition. */
    Config cfg;
    DoorMgr d(cfg);
    d.set_door_open(true);

    d.request_unlock();
    REQUIRE(d.state() == DoorState::Welcome);
}

TEST_CASE("Lock and unlock request chirps", "[door][horn]")
{
    /* SRS-HORN-002. */
    Config cfg;
    DoorMgr d(cfg);

    d.request_unlock();
    REQUIRE(d.take_chirp_unlock());
    REQUIRE_FALSE(d.take_chirp_unlock());   // one-shot

    d.update(cfg.welcome_duration_ms);
    d.request_lock();
    REQUIRE(d.take_chirp_lock());
}

TEST_CASE("A refused lock does not chirp", "[door]")
{
    /* Chirping would signal success the driver did not get. */
    Config cfg;
    DoorMgr d(cfg);
    d.request_unlock();
    d.update(cfg.welcome_duration_ms);
    (void)d.take_chirp_unlock();

    d.set_door_open(true);
    d.request_lock();
    REQUIRE_FALSE(d.take_chirp_lock());
}

TEST_CASE("The refusal flag is a one-shot", "[door]")
{
    Config cfg;
    DoorMgr d(cfg);
    d.request_unlock();
    d.update(cfg.welcome_duration_ms);

    d.set_door_open(true);
    d.request_lock();
    REQUIRE(d.take_lock_refused());
    REQUIRE_FALSE(d.take_lock_refused());
}

TEST_CASE("Locking while already locked is a no-op", "[door]")
{
    Config cfg;
    DoorMgr d(cfg);
    d.request_lock();
    REQUIRE(d.state() == DoorState::Locked);
    REQUIRE_FALSE(d.take_lock_refused());   // not a refusal, just nothing to do
}

TEST_CASE("reset() returns to locked", "[door]")
{
    Config cfg;
    DoorMgr d(cfg);
    d.request_unlock();

    d.reset();
    REQUIRE(d.state() == DoorState::Locked);
}
