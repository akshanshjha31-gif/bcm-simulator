/**
 * @file    test_arbiter.cpp
 * @brief   Unit tests for lamp arbitration - the safety layer.
 *
 * Traces SRS-BRK-001, SRS-REV-001, SRS-PWR-003/004, SRS-SAFETY-003/005/007.
 * These are the rules that must hold no matter what any feature asks for, so
 * they are tested against deliberately hostile lamp requests.
 */
#include "catch2/catch.hpp"
#include "lamp_arbiter.h"

using bcm::services::ArbiterInputs;
using bcm::services::LampArbiter;
using bcm::services::LampDrive;
using bcm::services::LampId;
using bcm::services::LampState;

namespace {
/// Every lamp demanding to be on - the worst case for arbitration.
LampState all_on()
{
    LampState l;
    for (uint8_t i = 0U; i < static_cast<uint8_t>(LampId::Count); ++i) {
        l.drive[i] = LampDrive::On;
    }
    return l;
}
}  // namespace

TEST_CASE("With nothing special asserted, requests pass through", "[arbiter]")
{
    LampState l;
    l.set(LampId::LowBeam, LampDrive::On);
    l.set(LampId::IndLeft, LampDrive::Blink);

    ArbiterInputs in;
    LampArbiter::arbitrate(l, in);

    REQUIRE(l.get(LampId::LowBeam) == LampDrive::On);
    REQUIRE(l.get(LampId::IndLeft) == LampDrive::Blink);
}

TEST_CASE("Brake pressed always lights the brake lamp", "[arbiter][safety]")
{
    /* SRS-BRK-001 / SAFETY-003: highest priority, overrides everything. */
    LampState l;                       // nothing requested at all
    ArbiterInputs in;
    in.brake_pressed = true;

    LampArbiter::arbitrate(l, in);
    REQUIRE(l.get(LampId::Brake) == LampDrive::On);
}

TEST_CASE("Brake wins even while load is being shed", "[arbiter][safety]")
{
    /* The dangerous interaction: a low battery must never take out the brake
     * lamp. Arbitration order is what guarantees this. */
    LampState l = all_on();
    ArbiterInputs in;
    in.brake_pressed = true;
    in.load_shed     = true;

    LampArbiter::arbitrate(l, in);
    REQUIRE(l.get(LampId::Brake) == LampDrive::On);
}

TEST_CASE("Brake wins even in Sleep", "[arbiter][safety]")
{
    LampState l = all_on();
    ArbiterInputs in;
    in.brake_pressed   = true;
    in.lamps_permitted = false;

    LampArbiter::arbitrate(l, in);
    REQUIRE(l.get(LampId::Brake) == LampDrive::On);
}

TEST_CASE("Brake released turns the brake lamp off", "[arbiter]")
{
    /* Even if something else asked for it - the brake output belongs to the
     * brake switch alone. */
    LampState l = all_on();
    ArbiterInputs in;
    in.brake_pressed = false;

    LampArbiter::arbitrate(l, in);
    REQUIRE(l.get(LampId::Brake) == LampDrive::Off);
}

TEST_CASE("Reverse lamp is refused without reverse gear", "[arbiter][safety]")
{
    /* SRS-REV-001 / SAFETY-005. */
    LampState l;
    l.set(LampId::Reverse, LampDrive::On);

    ArbiterInputs in;
    in.reverse_gear = false;

    LampArbiter::arbitrate(l, in);
    REQUIRE(l.get(LampId::Reverse) == LampDrive::Off);
}

TEST_CASE("Reverse lamp is allowed in reverse gear", "[arbiter]")
{
    LampState l;
    l.set(LampId::Reverse, LampDrive::On);

    ArbiterInputs in;
    in.reverse_gear = true;

    LampArbiter::arbitrate(l, in);
    REQUIRE(l.get(LampId::Reverse) == LampDrive::On);
}

TEST_CASE("Reverse gear alone does not light the lamp", "[arbiter]")
{
    /* Gear selection permits the lamp; it does not request it. */
    LampState l;
    ArbiterInputs in;
    in.reverse_gear = true;

    LampArbiter::arbitrate(l, in);
    REQUIRE(l.get(LampId::Reverse) == LampDrive::Off);
}

TEST_CASE("Sleep blanks the lamps except the lock indicator", "[arbiter][safety]")
{
    /* SRS-PWR-003. The lock lamp must survive so a parked car still shows
     * it is secured. */
    LampState l = all_on();
    ArbiterInputs in;
    in.lamps_permitted = false;

    LampArbiter::arbitrate(l, in);

    REQUIRE(l.get(LampId::DoorLock) == LampDrive::On);
    REQUIRE(l.get(LampId::LowBeam)  == LampDrive::Off);
    REQUIRE(l.get(LampId::HighBeam) == LampDrive::Off);
    REQUIRE(l.get(LampId::Drl)      == LampDrive::Off);
    REQUIRE(l.get(LampId::Ignition) == LampDrive::Off);
}

TEST_CASE("Load shedding drops comfort lamps but keeps safety lamps", "[arbiter][safety]")
{
    /* SRS-PWR-004. */
    LampState l = all_on();
    ArbiterInputs in;
    in.load_shed    = true;
    in.reverse_gear = true;

    LampArbiter::arbitrate(l, in);

    /* Shed: */
    REQUIRE(l.get(LampId::Drl)      == LampDrive::Off);
    REQUIRE(l.get(LampId::HighBeam) == LampDrive::Off);
    REQUIRE(l.get(LampId::DoorLock) == LampDrive::Off);

    /* Kept - other road users depend on these: */
    REQUIRE(l.get(LampId::LowBeam)  == LampDrive::On);
    REQUIRE(l.get(LampId::IndLeft)  == LampDrive::On);
    REQUIRE(l.get(LampId::Hazard)   == LampDrive::On);
    REQUIRE(l.get(LampId::Reverse)  == LampDrive::On);
}

TEST_CASE("The essential-lamp classification is explicit", "[arbiter]")
{
    REQUIRE(LampArbiter::is_essential(LampId::Brake));
    REQUIRE(LampArbiter::is_essential(LampId::Hazard));
    REQUIRE(LampArbiter::is_essential(LampId::IndLeft));
    REQUIRE(LampArbiter::is_essential(LampId::LowBeam));

    REQUIRE_FALSE(LampArbiter::is_essential(LampId::Drl));
    REQUIRE_FALSE(LampArbiter::is_essential(LampId::HighBeam));
    REQUIRE_FALSE(LampArbiter::is_essential(LampId::Ignition));
}

TEST_CASE("Losing comms drives a defined state, not the last command", "[arbiter][safety]")
{
    /* SRS-SAFETY-007: high beam and reverse must not be left latched on by a
     * host that has since disappeared. */
    LampState l = all_on();
    ArbiterInputs in;
    in.comms_lost   = true;
    in.reverse_gear = true;

    LampArbiter::arbitrate(l, in);
    REQUIRE(l.get(LampId::HighBeam) == LampDrive::Off);
    REQUIRE(l.get(LampId::Reverse)  == LampDrive::Off);
}

TEST_CASE("Hazards survive a comms loss", "[arbiter][safety]")
{
    /* A stranded vehicle should stay conspicuous. */
    LampState l;
    l.set(LampId::Hazard,   LampDrive::Blink);
    l.set(LampId::IndLeft,  LampDrive::Blink);
    l.set(LampId::IndRight, LampDrive::Blink);

    ArbiterInputs in;
    in.comms_lost = true;

    LampArbiter::arbitrate(l, in);
    REQUIRE(l.get(LampId::Hazard)   == LampDrive::Blink);
    REQUIRE(l.get(LampId::IndLeft)  == LampDrive::Blink);
}

TEST_CASE("Arbitration is idempotent", "[arbiter]")
{
    /* It runs every cycle, so a second pass must not change the outcome. */
    LampState once = all_on();
    ArbiterInputs in;
    in.brake_pressed = true;
    in.load_shed     = true;
    in.reverse_gear  = true;

    LampArbiter::arbitrate(once, in);
    LampState twice = once;
    LampArbiter::arbitrate(twice, in);

    for (uint8_t i = 0U; i < static_cast<uint8_t>(LampId::Count); ++i) {
        REQUIRE(once.drive[i] == twice.drive[i]);
    }
}

TEST_CASE("All safety rules hold together under the worst case", "[arbiter][safety]")
{
    /* Everything asserted at once: sleep, shed, comms lost, brake pressed,
     * no reverse gear - and every lamp demanding to be lit. */
    LampState l = all_on();
    ArbiterInputs in;
    in.lamps_permitted = false;
    in.load_shed       = true;
    in.comms_lost      = true;
    in.brake_pressed   = true;
    in.reverse_gear    = false;

    LampArbiter::arbitrate(l, in);

    REQUIRE(l.get(LampId::Brake)    == LampDrive::On);    // still wins
    REQUIRE(l.get(LampId::Reverse)  == LampDrive::Off);   // no gear
    REQUIRE(l.get(LampId::HighBeam) == LampDrive::Off);
    REQUIRE(l.get(LampId::Drl)      == LampDrive::Off);
}
