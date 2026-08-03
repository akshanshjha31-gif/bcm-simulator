/**
 * @file    test_power_fault.cpp
 * @brief   Unit tests for the Power state machine and the DTC store.
 *
 * Traces SRS-PWR-001..004, SRS-DIAG-002.
 */
#include "catch2/catch.hpp"
#include "fault_mgr.h"
#include "power_mgr.h"

using bcm::services::Config;
using bcm::services::Dtc;
using bcm::services::FaultMgr;
using bcm::services::LampDrive;
using bcm::services::LampId;
using bcm::services::LampState;
using bcm::services::PowerMgr;
using bcm::services::PowerState;

namespace {
/// Run the machine forward in 10 ms steps.
void advance(PowerMgr& p, uint32_t ms)
{
    for (uint32_t t = 0U; t < ms; t += 10U) { p.update(10U); }
}
}  // namespace

TEST_CASE("Power starts asleep", "[power]")
{
    Config cfg;
    PowerMgr p(cfg);
    REQUIRE(p.state() == PowerState::Sleep);
    REQUIRE_FALSE(p.running());
    REQUIRE_FALSE(p.lamps_permitted());
}

TEST_CASE("Ignition wakes the BCM and it reaches Run", "[power]")
{
    Config cfg;
    PowerMgr p(cfg);

    p.ignition_on();
    REQUIRE(p.state() == PowerState::Wake);

    advance(p, 300U);
    REQUIRE(p.state() == PowerState::Run);
    REQUIRE(p.running());
    REQUIRE(p.lamps_permitted());
}

TEST_CASE("Ignition off shuts down and returns to Sleep", "[power]")
{
    Config cfg;
    PowerMgr p(cfg);
    p.ignition_on();
    advance(p, 300U);

    p.ignition_off();
    REQUIRE(p.state() == PowerState::Shutdown);

    advance(p, 600U);
    REQUIRE(p.state() == PowerState::Sleep);
}

TEST_CASE("Ignition during shutdown aborts it", "[power]")
{
    /* The driver's intent wins over an in-progress shutdown. */
    Config cfg;
    PowerMgr p(cfg);
    p.ignition_on();
    advance(p, 300U);
    p.ignition_off();
    REQUIRE(p.state() == PowerState::Shutdown);

    p.ignition_on();
    REQUIRE(p.state() == PowerState::Run);
}

TEST_CASE("The ignition lamp follows the power state", "[power]")
{
    Config cfg;
    PowerMgr p(cfg);

    LampState asleep;
    p.apply(asleep);
    REQUIRE(asleep.get(LampId::Ignition) == LampDrive::Off);

    p.ignition_on();
    advance(p, 300U);
    LampState running;
    p.apply(running);
    REQUIRE(running.get(LampId::Ignition) == LampDrive::On);
}

TEST_CASE("Inactivity eventually puts the BCM to sleep", "[power]")
{
    /* SRS-PWR-002. */
    Config cfg;
    cfg.sleep_after_ms = 1000U;
    PowerMgr p(cfg);

    p.ignition_on();
    advance(p, 300U);
    REQUIRE(p.running());

    advance(p, 1200U);
    REQUIRE(p.state() == PowerState::Shutdown);

    advance(p, 600U);
    REQUIRE(p.state() == PowerState::Sleep);
}

TEST_CASE("A low battery sheds load", "[power][safety]")
{
    /* SRS-PWR-004. */
    Config cfg;
    PowerMgr p(cfg);

    p.update_battery(800U);           // first reading primes only
    REQUIRE_FALSE(p.load_shed());

    p.update_battery(100U);
    REQUIRE(p.load_shed());
}

TEST_CASE("Load shedding recovers only above the higher threshold", "[power]")
{
    /* Hysteresis stops a sagging battery oscillating in and out of shed. */
    Config cfg;
    PowerMgr p(cfg);
    p.update_battery(800U);
    p.update_battery(100U);
    REQUIRE(p.load_shed());

    p.update_battery(320U);           // between low(300) and ok(350)
    REQUIRE(p.load_shed());           // still shedding

    p.update_battery(400U);
    REQUIRE_FALSE(p.load_shed());
}

TEST_CASE("The first battery reading cannot cause a spurious shed", "[power]")
{
    /* At power-up the ADC filter has not settled; blanking the lamps because
     * of one unsettled sample would be a visible fault. */
    Config cfg;
    PowerMgr p(cfg);

    p.update_battery(0U);
    REQUIRE_FALSE(p.load_shed());
}

/* ---- Fault manager ----------------------------------------------------- */

TEST_CASE("A new fault store is empty", "[fault]")
{
    FaultMgr f;
    REQUIRE(f.count() == 0U);
    REQUIRE(f.active_count() == 0U);
    REQUIRE_FALSE(f.is_active(Dtc::BatteryLow));
}

TEST_CASE("Setting a DTC records it as active", "[fault]")
{
    FaultMgr f;
    f.set(Dtc::BatteryLow);

    REQUIRE(f.is_active(Dtc::BatteryLow));
    REQUIRE(f.count() == 1U);
    REQUIRE(f.active_count() == 1U);
}

TEST_CASE("A repeated DTC counts occurrences rather than duplicating", "[fault]")
{
    /* A chattering fault must not flush the others out of a fixed table. */
    FaultMgr f;
    for (int i = 0; i < 50; ++i) { f.set(Dtc::CommsLost); }

    REQUIRE(f.count() == 1U);
    REQUIRE(f.at(0).occurrences == 50U);
}

TEST_CASE("Clearing deactivates but retains history", "[fault]")
{
    FaultMgr f;
    f.set(Dtc::BatteryLow);
    f.clear(Dtc::BatteryLow);

    REQUIRE_FALSE(f.is_active(Dtc::BatteryLow));
    REQUIRE(f.count() == 1U);          // history kept
    REQUIRE(f.active_count() == 0U);
}

TEST_CASE("clear_all wipes the store", "[fault]")
{
    FaultMgr f;
    f.set(Dtc::BatteryLow);
    f.set(Dtc::CommsLost);

    f.clear_all();
    REQUIRE(f.count() == 0U);
    REQUIRE_FALSE(f.is_active(Dtc::CommsLost));
}

TEST_CASE("The store does not overflow", "[fault]")
{
    FaultMgr f;
    for (uint8_t i = 1U; i < 30U; ++i) { f.set(static_cast<Dtc>(i)); }
    REQUIRE(f.count() <= FaultMgr::kMaxDtc);
}

TEST_CASE("Dtc::None is ignored", "[fault]")
{
    FaultMgr f;
    f.set(Dtc::None);
    REQUIRE(f.count() == 0U);
}

TEST_CASE("Active codes serialise for GET_DTC", "[fault]")
{
    FaultMgr f;
    f.set(Dtc::BatteryLow);
    f.set(Dtc::CommsLost);
    f.clear(Dtc::BatteryLow);

    uint8_t buf[8] = {};
    const uint8_t n = f.active_codes(buf, 8U);

    REQUIRE(n == 1U);
    REQUIRE(buf[0] == static_cast<uint8_t>(Dtc::CommsLost));
}

TEST_CASE("Serialisation respects the caller's capacity", "[fault]")
{
    FaultMgr f;
    f.set(Dtc::BatteryLow);
    f.set(Dtc::CommsLost);
    f.set(Dtc::UartOverrun);

    uint8_t buf[2] = {};
    REQUIRE(f.active_codes(buf, 2U) == 2U);
    REQUIRE(f.active_codes(0, 8U) == 0U);   // null is handled, not crashed
}
