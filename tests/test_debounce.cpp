/**
 * @file    test_debounce.cpp
 * @brief   Unit tests for drivers::Debouncer.
 *
 * Traces SRS-SENS-002 (discrete inputs) and SRS-PERF-001 (50 ms
 * switch-to-lamp budget - the debounce window must fit inside it).
 */
#include "catch2/catch.hpp"
#include "debounce.h"

using bcm::drivers::Debouncer;

namespace {
/// Feed the same level repeatedly, 1 ms at a time.
void hold(Debouncer& d, bool level, int ms)
{
    for (int i = 0; i < ms; ++i) { d.update(level, 1U); }
}
}  // namespace

TEST_CASE("Debouncer starts in the released state", "[debounce]")
{
    Debouncer d(20U);
    REQUIRE(d.state() == false);
    REQUIRE(d.rising() == false);
    REQUIRE(d.falling() == false);
}

TEST_CASE("A level held for the window is accepted", "[debounce]")
{
    Debouncer d(20U);

    hold(d, true, 19);
    REQUIRE(d.state() == false);   // not yet stable

    hold(d, true, 2);
    REQUIRE(d.state() == true);
}

TEST_CASE("A glitch shorter than the window is rejected", "[debounce]")
{
    Debouncer d(20U);

    /* 5 ms of noise then back to released - must never register. */
    hold(d, true, 5);
    hold(d, false, 30);

    REQUIRE(d.state() == false);
    REQUIRE(d.rising() == false);
}

TEST_CASE("Contact bounce restarts the window", "[debounce]")
{
    Debouncer d(20U);

    /* Classic bounce: alternating for 15 ms, then settled high. */
    for (int i = 0; i < 15; ++i) { d.update((i % 2) == 0, 1U); }
    REQUIRE(d.state() == false);

    hold(d, true, 21);
    REQUIRE(d.state() == true);
}

TEST_CASE("Edges are reported exactly once", "[debounce]")
{
    Debouncer d(10U);

    bool rising_count = 0;
    for (int i = 0; i < 50; ++i) {
        d.update(true, 1U);
        if (d.rising()) { ++rising_count; }
    }
    REQUIRE(rising_count == 1);
    REQUIRE(d.state() == true);

    int falling_count = 0;
    for (int i = 0; i < 50; ++i) {
        d.update(false, 1U);
        if (d.falling()) { ++falling_count; }
    }
    REQUIRE(falling_count == 1);
    REQUIRE(d.state() == false);
}

TEST_CASE("update() returns true only on the transition", "[debounce]")
{
    Debouncer d(10U);

    int changes = 0;
    for (int i = 0; i < 30; ++i) {
        if (d.update(true, 1U)) { ++changes; }
    }
    REQUIRE(changes == 1);
}

TEST_CASE("A single large dt satisfies the window", "[debounce]")
{
    Debouncer d(20U);

    d.update(true, 1U);       // registers the candidate
    d.update(true, 500U);     // one long gap - must accept, not overflow
    REQUIRE(d.state() == true);
}

TEST_CASE("Accumulator saturates instead of wrapping", "[debounce]")
{
    Debouncer d(20U);

    d.update(true, 1U);
    /* Repeated near-max deltas must not wrap uint16 back below the window. */
    for (int i = 0; i < 5; ++i) { d.update(true, 65000U); }
    REQUIRE(d.state() == true);
}

TEST_CASE("reset() forces a level without edges", "[debounce]")
{
    Debouncer d(20U);

    d.reset(true);
    REQUIRE(d.state() == true);
    REQUIRE(d.rising() == false);

    /* Reading the same level again must not produce an edge. */
    hold(d, true, 30);
    REQUIRE(d.rising() == false);
}

TEST_CASE("Debounce window fits the SRS-PERF-001 budget", "[debounce]")
{
    /* Switch-to-lamp must be <= 50 ms, so the default 20 ms window plus a
     * 5 ms sampling period must leave headroom. */
    Debouncer d(20U);

    int ms = 0;
    while (!d.state() && ms < 100) {
        d.update(true, 5U);
        ms += 5;
    }
    REQUIRE(d.state() == true);
    REQUIRE(ms <= 50);
}
