/**
 * @file    test_lighting.cpp
 * @brief   Unit tests for the Lighting and Indicator state machines.
 *
 * Traces SRS-LIGHT-001..004 and SRS-IND-001..004.
 */
#include "catch2/catch.hpp"
#include "lighting_mgr.h"

using bcm::services::Config;
using bcm::services::IndState;
using bcm::services::LampDrive;
using bcm::services::LampId;
using bcm::services::LampState;
using bcm::services::LightingMgr;
using bcm::services::LightState;

namespace {
LampState lamps_of(const LightingMgr& m)
{
    LampState l;
    m.apply(l);
    return l;
}
}  // namespace

TEST_CASE("Lighting starts off", "[lighting]")
{
    Config cfg;
    LightingMgr m(cfg);
    REQUIRE(m.light_state() == LightState::Off);
    REQUIRE(lamps_of(m).bitmap() == 0U);
}

TEST_CASE("The light switch steps through every mode and wraps", "[lighting]")
{
    /* SRS-LIGHT-001: Off -> Parking -> DRL -> Low -> High. */
    Config cfg;
    LightingMgr m(cfg);

    m.next_mode(); REQUIRE(m.light_state() == LightState::Parking);
    m.next_mode(); REQUIRE(m.light_state() == LightState::Drl);
    m.next_mode(); REQUIRE(m.light_state() == LightState::LowBeam);
    m.next_mode(); REQUIRE(m.light_state() == LightState::HighBeam);
    m.next_mode(); REQUIRE(m.light_state() == LightState::Off);
}

TEST_CASE("High beam never runs without low beam beneath it", "[lighting]")
{
    Config cfg;
    LightingMgr m(cfg);
    for (int i = 0; i < 4; ++i) { m.next_mode(); }
    REQUIRE(m.light_state() == LightState::HighBeam);

    const LampState l = lamps_of(m);
    REQUIRE(l.get(LampId::HighBeam) == LampDrive::On);
    REQUIRE(l.get(LampId::LowBeam)  == LampDrive::On);
}

TEST_CASE("all_off blanks the lighting regardless of mode", "[lighting]")
{
    Config cfg;
    LightingMgr m(cfg);
    m.next_mode(); m.next_mode(); m.next_mode();

    m.all_off();
    REQUIRE(m.light_state() == LightState::Off);
    REQUIRE(lamps_of(m).bitmap() == 0U);
}

TEST_CASE("Auto-headlight switches low beam on in the dark", "[lighting]")
{
    /* SRS-LIGHT-003. */
    Config cfg;
    LightingMgr m(cfg);

    m.update_ambient(900U);                       // bright
    REQUIRE(m.light_state() == LightState::Off);

    m.update_ambient(100U);                       // dark
    REQUIRE(m.light_state() == LightState::LowBeam);
    REQUIRE(m.auto_headlight());
}

TEST_CASE("Auto-headlight releases when it gets light again", "[lighting]")
{
    Config cfg;
    LightingMgr m(cfg);

    m.update_ambient(900U);
    m.update_ambient(100U);
    REQUIRE(m.light_state() == LightState::LowBeam);

    m.update_ambient(950U);
    REQUIRE(m.light_state() == LightState::Off);
    REQUIRE_FALSE(m.auto_headlight());
}

TEST_CASE("Auto-headlight does not chatter at dusk", "[lighting]")
{
    /* Hysteresis: readings hovering between the thresholds must not toggle
     * the headlights repeatedly. */
    Config cfg;
    LightingMgr m(cfg);
    m.update_ambient(900U);
    m.update_ambient(100U);                       // latch on

    const LightState before = m.light_state();
    const uint16_t noise[] = { 620U, 680U, 650U, 660U, 640U, 670U };
    for (uint16_t v : noise) { m.update_ambient(v); }

    REQUIRE(m.light_state() == before);
}

TEST_CASE("A manual choice suppresses auto-headlight", "[lighting]")
{
    /* The driver picking a mode must not be overridden by the sensor. */
    Config cfg;
    LightingMgr m(cfg);

    m.next_mode();                                // Parking, by hand
    REQUIRE(m.light_state() == LightState::Parking);

    m.update_ambient(900U);
    m.update_ambient(50U);                        // very dark
    REQUIRE(m.light_state() == LightState::Parking);
}

TEST_CASE("Indicators start idle and toggle", "[indicator]")
{
    Config cfg;
    LightingMgr m(cfg);
    REQUIRE(m.indicator_state() == IndState::Idle);

    m.indicator_left();
    REQUIRE(m.indicator_state() == IndState::Left);

    m.indicator_left();                           // press again cancels
    REQUIRE(m.indicator_state() == IndState::Idle);
}

TEST_CASE("Selecting the opposite indicator switches directly", "[indicator]")
{
    Config cfg;
    LightingMgr m(cfg);

    m.indicator_left();
    m.indicator_right();
    REQUIRE(m.indicator_state() == IndState::Right);
}

TEST_CASE("An active indicator blinks rather than sitting on", "[indicator]")
{
    /* SRS-IND-001: indicators flash. */
    Config cfg;
    LightingMgr m(cfg);
    m.indicator_left();

    const LampState l = lamps_of(m);
    REQUIRE(l.get(LampId::IndLeft)  == LampDrive::Blink);
    REQUIRE(l.get(LampId::IndRight) == LampDrive::Off);
}

TEST_CASE("Hazard overrides a running indicator", "[indicator]")
{
    /* SRS-IND-003 - the load-bearing rule for this FSM. */
    Config cfg;
    LightingMgr m(cfg);

    m.indicator_left();
    REQUIRE(m.indicator_state() == IndState::Left);

    m.indicator_hazard();
    REQUIRE(m.indicator_state() == IndState::Hazard);

    const LampState l = lamps_of(m);
    REQUIRE(l.get(LampId::IndLeft)  == LampDrive::Blink);
    REQUIRE(l.get(LampId::IndRight) == LampDrive::Blink);
    REQUIRE(l.get(LampId::Hazard)   == LampDrive::Blink);
}

TEST_CASE("Indicators cannot pre-empt hazard", "[indicator]")
{
    /* While hazard is latched, a direction request must be refused - both
     * sides stay flashing. */
    Config cfg;
    LightingMgr m(cfg);

    m.indicator_hazard();
    REQUIRE(m.indicator_state() == IndState::Hazard);

    m.indicator_left();
    REQUIRE(m.indicator_state() == IndState::Hazard);

    m.indicator_right();
    REQUIRE(m.indicator_state() == IndState::Hazard);
}

TEST_CASE("Hazard is a toggle", "[indicator]")
{
    Config cfg;
    LightingMgr m(cfg);

    m.indicator_hazard();
    REQUIRE(m.indicator_state() == IndState::Hazard);

    m.indicator_hazard();
    REQUIRE(m.indicator_state() == IndState::Idle);

    /* And directional control must work again afterwards. */
    m.indicator_left();
    REQUIRE(m.indicator_state() == IndState::Left);
}

TEST_CASE("Lighting and indicators are independent", "[lighting][indicator]")
{
    Config cfg;
    LightingMgr m(cfg);

    m.next_mode(); m.next_mode(); m.next_mode();   // LowBeam
    m.indicator_hazard();

    const LampState l = lamps_of(m);
    REQUIRE(l.get(LampId::LowBeam) == LampDrive::On);
    REQUIRE(l.get(LampId::Hazard)  == LampDrive::Blink);
}

TEST_CASE("reset() returns both machines to their initial state", "[lighting]")
{
    Config cfg;
    LightingMgr m(cfg);
    m.next_mode();
    m.indicator_hazard();

    m.reset();
    REQUIRE(m.light_state() == LightState::Off);
    REQUIRE(m.indicator_state() == IndState::Idle);
}
