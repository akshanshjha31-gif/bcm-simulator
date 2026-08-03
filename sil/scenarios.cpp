/**
 * @file    scenarios.cpp
 * @brief   SIL scenarios - whole-vehicle behaviour over simulated time.
 *
 * These are deliberately NOT unit tests. The unit suite proves each module in
 * isolation; these drive the assembled BCM the way a person would and check
 * what a person would see, including the interactions between managers that
 * no single unit test can reach.
 */
#include "scenarios.h"

namespace bcm {
namespace sil {

using services::Dtc;
using services::DoorState;
using services::IndState;
using services::LampId;
using services::LightState;
using services::PowerState;

namespace {

/* ---- Power ------------------------------------------------------------- */

void scenario_startup(SilRunner& r)
{
    r.begin("Cold start is dark and asleep", "SRS-PWR-001, SRS-PWR-003");

    r.expect_power(PowerState::Sleep, "BCM starts asleep");
    r.expect_lamp(LampId::Ignition, false, "ignition lamp off");
    r.expect_lamp(LampId::LowBeam,  false, "low beam off");
    r.expect_lamp(LampId::Brake,    false, "brake lamp off");
    r.expect_lamp(LampId::DoorLock, true,  "doors start locked, lamp lit");
}

void scenario_ignition_cycle(SilRunner& r)
{
    r.begin("Ignition wakes and shuts down the BCM", "SRS-PWR-001");

    r.tap("ignition");
    r.advance(400U);
    r.expect_power(PowerState::Run, "reaches Run after Wake");
    r.expect_lamp(LampId::Ignition, true, "ignition lamp lit");

    r.tap("ignition");
    r.advance(800U);
    r.expect_power(PowerState::Sleep, "returns to Sleep");
    r.expect_lamp(LampId::Ignition, false, "ignition lamp out");
}

/* ---- Lighting ----------------------------------------------------------- */

void scenario_light_cycle(SilRunner& r)
{
    r.begin("Light switch steps through every mode", "SRS-LIGHT-001");

    r.tap("ignition");
    r.advance(400U);

    r.next_light_mode();
    r.expect_light_state(LightState::Parking, "Off -> Parking");

    r.next_light_mode();
    r.expect_light_state(LightState::Drl, "Parking -> DRL");

    r.next_light_mode();
    r.expect_light_state(LightState::LowBeam, "DRL -> Low beam");
    r.expect_lamp(LampId::LowBeam, true, "low beam lit");

    r.next_light_mode();
    r.expect_light_state(LightState::HighBeam, "Low -> High beam");
    r.expect_lamp(LampId::HighBeam, true, "high beam lit");
    r.expect_lamp(LampId::LowBeam,  true, "low beam stays lit beneath high");

    r.next_light_mode();
    r.expect_light_state(LightState::Off, "High beam wraps to Off");
}

void scenario_long_press_lights(SilRunner& r)
{
    r.begin("Holding ignition steps the lights, tapping does not",
            "bring-up affordance until a light switch is wired");

    r.tap("ignition");
    r.advance(400U);
    r.expect_light_state(LightState::Off, "a short press did not touch the lights");

    r.press("ignition");
    r.advance(1000U);           /* longer than long_press_ms */
    r.release("ignition");
    r.expect_light_state(LightState::Parking, "a long press stepped the mode");
    r.expect_power(PowerState::Run, "and did NOT toggle the ignition");
}

void scenario_auto_headlight(SilRunner& r)
{
    r.begin("Auto-headlight follows ambient light", "SRS-LIGHT-003");

    r.tap("ignition");
    r.advance(400U);

    r.set_ambient(900U);
    r.advance(100U);
    r.expect_lamp(LampId::LowBeam, false, "daylight leaves the beam off");

    r.set_ambient(80U);
    r.advance(100U);
    r.expect_lamp(LampId::LowBeam, true, "darkness switches low beam on");

    /* Dusk: readings hovering between the thresholds must not chatter. */
    const LightState settled = r.core().light_state();
    for (int i = 0; i < 6; ++i) {
        r.set_ambient((i % 2) ? 620U : 680U);
        r.advance(50U);
    }
    r.expect(r.core().light_state() == settled,
             "hysteresis keeps the beam steady at dusk");

    r.set_ambient(950U);
    r.advance(100U);
    r.expect_lamp(LampId::LowBeam, false, "daylight releases it again");
}

/* ---- Indicators --------------------------------------------------------- */

void scenario_indicators(SilRunner& r)
{
    r.begin("Indicators flash and cancel", "SRS-IND-001, SRS-IND-004");

    r.tap("ignition");
    r.advance(400U);

    r.tap("left");
    r.expect_indicator(IndState::Left, "left indicator active");
    r.expect_lamp_blinking(LampId::IndLeft, "left lamp flashes");
    r.expect_lamp(LampId::IndRight, false, "right lamp stays off");

    r.tap("right");
    r.expect_indicator(IndState::Right, "right takes over directly");

    r.tap("right");
    r.expect_indicator(IndState::Idle, "pressing again cancels");
}

void scenario_hazard_override(SilRunner& r)
{
    r.begin("Hazard overrides a running indicator", "SRS-IND-003");

    r.tap("ignition");
    r.advance(400U);

    r.tap("left");
    r.expect_indicator(IndState::Left, "left indicator running");

    r.tap("hazard");
    r.expect_indicator(IndState::Hazard, "hazard takes over");
    r.expect_lamp_blinking(LampId::IndLeft,  "both sides flash - left");
    r.expect_lamp_blinking(LampId::IndRight, "both sides flash - right");

    /* And a direction request must not be able to pre-empt it. */
    r.tap("right");
    r.expect_indicator(IndState::Hazard, "indicators cannot pre-empt hazard");

    r.tap("hazard");
    r.expect_indicator(IndState::Idle, "hazard toggles off");
}

/* ---- Safety-critical ---------------------------------------------------- */

void scenario_brake_priority(SilRunner& r)
{
    r.begin("Brake lamp overrides everything", "SRS-BRK-001, SRS-SAFETY-003");

    r.tap("ignition");
    r.advance(400U);
    r.next_light_mode(); r.next_light_mode();
    r.next_light_mode(); r.next_light_mode();     /* high beam */
    r.tap("hazard");

    r.press("brake");
    r.advance(50U);
    r.expect_lamp(LampId::Brake, true, "brake lamp lit with everything else on");

    /* Now the dangerous combination: a flat battery must not take it out. */
    r.set_battery(80U);
    r.advance(200U);
    r.expect(r.core().load_shed(), "load shedding is active");
    r.expect_lamp(LampId::Brake, true, "brake survives load shedding");
    r.expect_lamp(LampId::HighBeam, false, "high beam is shed");

    r.release("brake");
    r.advance(50U);
    r.expect_lamp(LampId::Brake, false, "brake lamp follows the pedal");
}

void scenario_reverse_gate(SilRunner& r)
{
    r.begin("Reverse lamp only in reverse gear",
            "SRS-REV-001, SRS-SAFETY-005");

    r.tap("ignition");
    r.advance(400U);

    /* Ask for it over the diagnostic link with no gear selected. */
    services::LampState request;
    request.set(LampId::Reverse, services::LampDrive::On);
    r.core().set_host_request(request, 1U << static_cast<uint8_t>(LampId::Reverse));
    r.advance(50U);
    r.expect_lamp(LampId::Reverse, false,
                  "a diagnostic host cannot light it without gear");

    r.set_reverse_gear(true);
    r.advance(50U);
    r.expect_lamp(LampId::Reverse, true, "gear engaged, lamp permitted");

    r.set_reverse_gear(false);
    r.advance(50U);
    r.expect_lamp(LampId::Reverse, false, "leaving reverse extinguishes it");
}

void scenario_door_open_guard(SilRunner& r)
{
    r.begin("Doors never lock while one is open",
            "SRS-DOOR-002, SRS-SAFETY-004");

    r.tap("ignition");
    r.advance(400U);

    r.tap("lock");                       /* unlock */
    r.advance(2500U);                    /* let welcome expire */
    r.expect_door(DoorState::Unlocked, "doors unlocked");

    r.set_door_open(true);
    r.tap("lock");
    r.expect_door(DoorState::Unlocked, "manual lock refused with a door open");
    r.expect_dtc(Dtc::LockRefusedDoorOpen, true, "and a DTC is raised");

    /* Auto-lock must be blocked too - a rule enforced only on the manual
     * path is not enforced at all. */
    r.advance(8000U);
    r.expect_door(DoorState::Unlocked, "auto-lock also refused");

    r.set_door_open(false);
    r.advance(4000U);
    r.expect_door(DoorState::Locked, "locks once the door is shut");
}

void scenario_auto_lock(SilRunner& r)
{
    r.begin("Auto-lock engages after the configured delay", "SRS-DOOR-003");

    r.tap("ignition");
    r.advance(400U);

    r.tap("lock");                       /* unlock -> welcome */
    r.expect_door(DoorState::Welcome, "welcome phase runs");
    r.expect_lamp(LampId::Drl, true, "welcome lighting is visible");

    r.advance(2500U);
    r.expect_door(DoorState::Unlocked, "welcome expires");

    r.advance(3500U);
    r.expect_door(DoorState::Locked, "auto-locked");
    r.expect_lamp(LampId::DoorLock, true, "lock lamp lit");
}

void scenario_comms_loss(SilRunner& r)
{
    r.begin("Losing the diagnostic link drives a defined state",
            "SRS-SAFETY-007");

    r.tap("ignition");
    r.advance(400U);
    r.next_light_mode(); r.next_light_mode();
    r.next_light_mode(); r.next_light_mode();     /* high beam */
    r.tap("hazard");
    r.expect_lamp(LampId::HighBeam, true, "high beam on before the fault");

    r.set_comms_lost(true);
    r.advance(100U);

    r.expect_lamp(LampId::HighBeam, false, "high beam dropped to a safe state");
    r.expect_lamp_blinking(LampId::Hazard,
                           "hazards keep flashing - the vehicle stays visible");
    r.expect_dtc(Dtc::CommsLost, true, "comms DTC raised");

    r.set_comms_lost(false);
    r.advance(100U);
    r.expect_dtc(Dtc::CommsLost, false, "DTC clears when the link returns");
}

void scenario_battery_recovery(SilRunner& r)
{
    r.begin("Load shedding recovers with hysteresis", "SRS-PWR-004");

    r.tap("ignition");
    r.advance(400U);
    r.next_light_mode(); r.next_light_mode();     /* DRL */

    r.set_battery(80U);
    r.advance(200U);
    r.expect(r.core().load_shed(), "low battery sheds load");
    r.expect_lamp(LampId::Drl, false, "DRL is shed");
    r.expect_dtc(Dtc::BatteryLow, true, "battery DTC raised");

    /* Between the thresholds: must stay shed, not oscillate. */
    r.set_battery(320U);
    r.advance(200U);
    r.expect(r.core().load_shed(), "still shed between the thresholds");

    r.set_battery(600U);
    r.advance(200U);
    r.expect(!r.core().load_shed(), "recovers above the upper threshold");
    r.expect_dtc(Dtc::BatteryLow, false, "battery DTC clears");
}

void scenario_sleep_blanks_lamps(SilRunner& r)
{
    r.begin("Sleep blanks every lamp but the lock indicator", "SRS-PWR-003");

    r.tap("ignition");
    r.advance(400U);
    r.next_light_mode(); r.next_light_mode();
    r.next_light_mode();                          /* low beam */
    r.expect_lamp(LampId::LowBeam, true, "low beam on while running");

    r.tap("ignition");                            /* shut down */
    r.advance(900U);

    r.expect_power(PowerState::Sleep, "asleep");
    r.expect_lamp(LampId::LowBeam,  false, "low beam blanked");
    r.expect_lamp(LampId::Drl,      false, "DRL blanked");
    r.expect_lamp(LampId::Ignition, false, "ignition lamp blanked");
    r.expect_lamp(LampId::DoorLock, true,
                  "lock indicator survives so a parked car shows it is secured");
}

/* ---- Composite ---------------------------------------------------------- */

void scenario_full_drive(SilRunner& r)
{
    r.begin("A complete drive cycle", "composite");

    /* Approach the car and unlock. */
    r.tap("lock");
    r.expect_door(DoorState::Welcome, "welcome lighting on approach");
    r.advance(2500U);

    /* Start up, dusk falls, headlights come on by themselves. */
    r.tap("ignition");
    r.advance(400U);
    r.expect_power(PowerState::Run, "engine running");

    r.set_ambient(100U);
    r.advance(200U);
    r.expect_lamp(LampId::LowBeam, true, "auto-headlight at dusk");

    /* Indicate, brake, reverse into a space. */
    r.tap("left");
    r.expect_lamp_blinking(LampId::IndLeft, "indicating left");

    r.press("brake");
    r.advance(100U);
    r.expect_lamp(LampId::Brake, true, "braking");

    r.set_reverse_gear(true);
    r.advance(50U);
    r.tap("hazard");
    r.expect_indicator(IndState::Hazard, "hazards on while manoeuvring");

    r.release("brake");
    r.set_reverse_gear(false);
    r.tap("hazard");
    r.advance(50U);

    /* Shut down and walk away. */
    r.tap("ignition");
    r.advance(900U);
    r.expect_power(PowerState::Sleep, "parked and asleep");
    r.expect_lamp(LampId::LowBeam, false, "headlights out");

    r.tap("lock");
    r.advance(100U);
    r.expect_door(DoorState::Locked, "locked on leaving");
}

}  // namespace

std::vector<Scenario> all_scenarios()
{
    return {
        { "startup",            scenario_startup },
        { "ignition-cycle",     scenario_ignition_cycle },
        { "light-cycle",        scenario_light_cycle },
        { "long-press-lights",  scenario_long_press_lights },
        { "auto-headlight",     scenario_auto_headlight },
        { "indicators",         scenario_indicators },
        { "hazard-override",    scenario_hazard_override },
        { "brake-priority",     scenario_brake_priority },
        { "reverse-gate",       scenario_reverse_gate },
        { "door-open-guard",    scenario_door_open_guard },
        { "auto-lock",          scenario_auto_lock },
        { "comms-loss",         scenario_comms_loss },
        { "battery-recovery",   scenario_battery_recovery },
        { "sleep-blanks-lamps", scenario_sleep_blanks_lamps },
        { "full-drive",         scenario_full_drive },
    };
}

}  // namespace sil
}  // namespace bcm
