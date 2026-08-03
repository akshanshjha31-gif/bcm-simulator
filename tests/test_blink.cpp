/**
 * @file    test_blink.cpp
 * @brief   Unit tests for drivers::Blinker.
 *
 * Traces SRS-IND-002 (indicator flash rate) and SRS-IND-004 (a lamp starts in
 * the ON phase - no dark first blink).
 */
#include "catch2/catch.hpp"
#include "blink.h"

using bcm::drivers::Blinker;

TEST_CASE("A new blinker is off", "[blink]")
{
    Blinker b;
    REQUIRE(b.output() == false);
    REQUIRE(b.mode() == Blinker::Mode::Off);
}

TEST_CASE("set_on / set_off drive the output immediately", "[blink]")
{
    Blinker b;

    b.set_on();
    REQUIRE(b.output() == true);
    REQUIRE(b.update(10000U) == true);   // steady state ignores time

    b.set_off();
    REQUIRE(b.output() == false);
    REQUIRE(b.update(10000U) == false);
}

TEST_CASE("Blinking starts lit (SRS-IND-004)", "[blink]")
{
    Blinker b;
    b.set_blink(333U, 333U);
    REQUIRE(b.output() == true);   // must not begin dark
}

TEST_CASE("Blink toggles at the configured period", "[blink]")
{
    Blinker b;
    b.set_blink(100U, 100U);

    REQUIRE(b.update(50U) == true);    // 50  - still on
    REQUIRE(b.update(50U) == false);   // 100 - switches off
    REQUIRE(b.update(50U) == false);   // 150 - still off
    REQUIRE(b.update(50U) == true);    // 200 - back on
}

TEST_CASE("Asymmetric on/off times are honoured", "[blink]")
{
    Blinker b;
    b.set_blink(300U, 100U);           // long on, short off

    REQUIRE(b.update(250U) == true);   // 250 - within the 300 ms on-phase
    REQUIRE(b.update(100U) == false);  // 350 - on-phase ended at 300
    REQUIRE(b.update(30U)  == false);  // 380 - off-phase runs 300..400
    REQUIRE(b.update(30U)  == true);   // 410 - off-phase ended at 400
}

TEST_CASE("A phase ends exactly on its boundary", "[blink]")
{
    /* Pins down the rule the asymmetric case depends on: reaching the limit
     * toggles, it does not wait for the next tick. */
    Blinker b;
    b.set_blink(100U, 100U);

    REQUIRE(b.update(99U)  == true);    // 99  - still on
    REQUIRE(b.update(1U)   == false);   // 100 - toggles on the boundary
}

TEST_CASE("A long dt catches up rather than leaving the phase stale", "[blink]")
{
    Blinker b;
    b.set_blink(100U, 100U);

    /* 250 ms is 2.5 periods: on->off->on, landing 50 ms into an on-phase. */
    REQUIRE(b.update(250U) == true);

    /* Only 50 ms of that on-phase remains. */
    REQUIRE(b.update(60U) == false);
}

TEST_CASE("Restarting with identical parameters preserves the phase", "[blink]")
{
    Blinker b;
    b.set_blink(100U, 100U);
    b.update(80U);

    /* Holding an indicator down re-issues the same request every pass; if it
     * reset the phase the lamp would stay permanently lit. */
    b.set_blink(100U, 100U);
    REQUIRE(b.update(30U) == false);   // 110 total - the toggle still happens
}

TEST_CASE("Changing the rate restarts the pattern", "[blink]")
{
    Blinker b;
    b.set_blink(100U, 100U);
    b.update(80U);

    b.set_blink(200U, 200U);
    REQUIRE(b.output() == true);       // restarted, and starts lit
    REQUIRE(b.update(150U) == true);   // fresh 200 ms on-phase
}

TEST_CASE("Zero period does not hang", "[blink]")
{
    Blinker b;
    b.set_blink(0U, 0U);
    REQUIRE(b.update(1000U) == b.output());   // terminates
}

TEST_CASE("Indicator rate is about 1.5 Hz", "[blink]")
{
    /* SRS-IND-002: 333 ms on / 333 ms off is one cycle per 666 ms. */
    Blinker b;
    b.set_blink(333U, 333U);

    int toggles = 0;
    bool prev = b.output();
    for (int ms = 0; ms < 6660; ms += 1) {     // 10 full cycles
        const bool now = b.update(1U);
        if (now != prev) { ++toggles; prev = now; }
    }
    /* 10 cycles = 20 transitions, allow one for boundary alignment. */
    REQUIRE(toggles >= 19);
    REQUIRE(toggles <= 21);
}
