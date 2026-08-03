/**
 * @file    sil_runner.cpp
 * @brief   Software-in-the-Loop fixture.
 */
#include "sil_runner.h"

#include <cstring>
#include <sstream>

namespace bcm {
namespace sil {
namespace {

/* Matches the firmware's Control task period, so timing-dependent behaviour
 * (debounce windows, blink phases, auto-lock) is exercised at the same
 * granularity the target runs at. */
const uint16_t kCyclePeriodMs = 5U;

const char* lamp_name(services::LampId id)
{
    switch (id) {
    case services::LampId::Ignition: return "Ignition";
    case services::LampId::Drl:      return "DRL";
    case services::LampId::LowBeam:  return "LowBeam";
    case services::LampId::HighBeam: return "HighBeam";
    case services::LampId::IndLeft:  return "IndLeft";
    case services::LampId::IndRight: return "IndRight";
    case services::LampId::Hazard:   return "Hazard";
    case services::LampId::Brake:    return "Brake";
    case services::LampId::Reverse:  return "Reverse";
    case services::LampId::DoorLock: return "DoorLock";
    default:                         return "?";
    }
}

const char* drive_name(services::LampDrive d)
{
    switch (d) {
    case services::LampDrive::On:    return "On";
    case services::LampDrive::Blink: return "Blink";
    default:                         return "Off";
    }
}

const char* light_name(services::LightState s)
{
    switch (s) {
    case services::LightState::Off:      return "Off";
    case services::LightState::Parking:  return "Parking";
    case services::LightState::Drl:      return "Drl";
    case services::LightState::LowBeam:  return "LowBeam";
    case services::LightState::HighBeam: return "HighBeam";
    default:                             return "?";
    }
}

const char* ind_name(services::IndState s)
{
    switch (s) {
    case services::IndState::Idle:   return "Idle";
    case services::IndState::Left:   return "Left";
    case services::IndState::Right:  return "Right";
    case services::IndState::Hazard: return "Hazard";
    default:                         return "?";
    }
}

const char* door_name(services::DoorState s)
{
    switch (s) {
    case services::DoorState::Locked:   return "Locked";
    case services::DoorState::Unlocked: return "Unlocked";
    case services::DoorState::Welcome:  return "Welcome";
    default:                            return "?";
    }
}

const char* power_name(services::PowerState s)
{
    switch (s) {
    case services::PowerState::Sleep:    return "Sleep";
    case services::PowerState::Wake:     return "Wake";
    case services::PowerState::Run:      return "Run";
    case services::PowerState::Shutdown: return "Shutdown";
    default:                             return "?";
    }
}

}  // namespace

bool ScenarioResult::passed() const
{
    for (const CheckResult& c : checks) {
        if (!c.passed) { return false; }
    }
    return !checks.empty();
}

unsigned ScenarioResult::failures() const
{
    unsigned n = 0U;
    for (const CheckResult& c : checks) {
        if (!c.passed) { ++n; }
    }
    return n;
}

SilRunner::SilRunner(const services::Config& config)
    : cfg_(config), core_(cfg_), inputs_(), now_ms_(0U), current_()
{
    /* A vehicle at rest: battery healthy, daylight, doors shut. */
    inputs_.battery_permille = 800U;
    inputs_.ambient_permille = 900U;
}

bool* SilRunner::control_ptr(const char* control)
{
    if (std::strcmp(control, "ignition") == 0)  { return &inputs_.ignition; }
    if (std::strcmp(control, "left") == 0)      { return &inputs_.ind_left; }
    if (std::strcmp(control, "right") == 0)     { return &inputs_.ind_right; }
    if (std::strcmp(control, "hazard") == 0)    { return &inputs_.hazard; }
    if (std::strcmp(control, "brake") == 0)     { return &inputs_.brake; }
    if (std::strcmp(control, "lock") == 0)      { return &inputs_.door_lock; }
    return 0;
}

void SilRunner::press(const char* control)
{
    bool* p = control_ptr(control);
    if (p == 0) {
        record(false, std::string("unknown control '") + control + "'", "");
        return;
    }
    *p = true;
    advance(kCyclePeriodMs);
}

void SilRunner::release(const char* control)
{
    bool* p = control_ptr(control);
    if (p == 0) {
        record(false, std::string("unknown control '") + control + "'", "");
        return;
    }
    *p = false;
    advance(kCyclePeriodMs);
}

void SilRunner::tap(const char* control, uint32_t hold_ms)
{
    press(control);
    advance(hold_ms);
    release(control);
}

void SilRunner::advance(uint32_t ms)
{
    uint32_t remaining = ms;
    while (remaining > 0U) {
        const uint16_t dt = (remaining >= kCyclePeriodMs)
                                ? kCyclePeriodMs
                                : static_cast<uint16_t>(remaining);
        core_.set_inputs(inputs_);
        core_.step(dt);
        now_ms_   += dt;
        remaining -= dt;
    }
}

void SilRunner::record(bool passed, const std::string& description,
                       const std::string& detail)
{
    CheckResult c;
    c.description = description;
    c.detail      = detail;
    c.at_ms       = now_ms_;
    c.passed      = passed;
    current_.checks.push_back(c);
}

void SilRunner::expect_lamp(services::LampId id, bool lit, const char* what)
{
    const services::LampDrive d = core_.lamps().get(id);
    const bool actual = (d != services::LampDrive::Off);

    std::ostringstream detail;
    if (actual != lit) {
        detail << lamp_name(id) << " is " << drive_name(d)
               << ", expected " << (lit ? "lit" : "off");
    }
    record(actual == lit, what, detail.str());
}

void SilRunner::expect_lamp_blinking(services::LampId id, const char* what)
{
    const services::LampDrive d = core_.lamps().get(id);

    std::ostringstream detail;
    if (d != services::LampDrive::Blink) {
        detail << lamp_name(id) << " is " << drive_name(d)
               << ", expected Blink";
    }
    record(d == services::LampDrive::Blink, what, detail.str());
}

void SilRunner::expect_light_state(services::LightState state, const char* what)
{
    std::ostringstream detail;
    if (core_.light_state() != state) {
        detail << "lighting is " << light_name(core_.light_state())
               << ", expected " << light_name(state);
    }
    record(core_.light_state() == state, what, detail.str());
}

void SilRunner::expect_indicator(services::IndState state, const char* what)
{
    std::ostringstream detail;
    if (core_.indicator_state() != state) {
        detail << "indicator is " << ind_name(core_.indicator_state())
               << ", expected " << ind_name(state);
    }
    record(core_.indicator_state() == state, what, detail.str());
}

void SilRunner::expect_door(services::DoorState state, const char* what)
{
    std::ostringstream detail;
    if (core_.door_state() != state) {
        detail << "door is " << door_name(core_.door_state())
               << ", expected " << door_name(state);
    }
    record(core_.door_state() == state, what, detail.str());
}

void SilRunner::expect_power(services::PowerState state, const char* what)
{
    std::ostringstream detail;
    if (core_.power_state() != state) {
        detail << "power is " << power_name(core_.power_state())
               << ", expected " << power_name(state);
    }
    record(core_.power_state() == state, what, detail.str());
}

void SilRunner::expect_dtc(services::Dtc code, bool active, const char* what)
{
    const bool actual = core_.faults().is_active(code);

    std::ostringstream detail;
    if (actual != active) {
        detail << "DTC 0x" << std::hex << static_cast<unsigned>(code)
               << " is " << (actual ? "active" : "inactive")
               << ", expected " << (active ? "active" : "inactive");
    }
    record(actual == active, what, detail.str());
}

void SilRunner::expect(bool condition, const char* what)
{
    record(condition, what, condition ? "" : "condition was false");
}

void SilRunner::begin(const char* name, const char* requirement)
{
    core_.reset();
    inputs_ = services::Inputs();
    inputs_.battery_permille = 800U;
    inputs_.ambient_permille = 900U;
    now_ms_ = 0U;

    current_ = ScenarioResult();
    current_.name        = name;
    current_.requirement = requirement;

    /* Let the core settle and seed its edge history. */
    advance(20U);
}

ScenarioResult SilRunner::end() { return current_; }

}  // namespace sil
}  // namespace bcm
