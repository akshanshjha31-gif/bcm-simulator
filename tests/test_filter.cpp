/**
 * @file    test_filter.cpp
 * @brief   Unit tests for drivers::ExpFilter and drivers::Hysteresis.
 *
 * Traces SRS-SENS-001 (filtered sensor values) and SRS-LIGHT-003
 * (auto-headlight switching with hysteresis).
 */
#include "catch2/catch.hpp"
#include "filter.h"

using bcm::drivers::ExpFilter;
using bcm::drivers::Hysteresis;

TEST_CASE("The filter seeds from the first sample", "[filter]")
{
    ExpFilter f(3U);

    /* Must not ramp up from zero, or every startup reads a false low battery. */
    REQUIRE(f.update(2000U) == 2000U);
}

TEST_CASE("A steady input holds its value", "[filter]")
{
    ExpFilter f(3U);
    f.update(1500U);

    for (int i = 0; i < 50; ++i) { f.update(1500U); }
    REQUIRE(f.value() == 1500U);
}

TEST_CASE("The filter converges toward a step change", "[filter]")
{
    ExpFilter f(3U);
    f.update(0U);

    for (int i = 0; i < 100; ++i) { f.update(1000U); }

    /* Should be close to the new level after many samples. */
    REQUIRE(f.value() > 990U);
    REQUIRE(f.value() <= 1000U);
}

TEST_CASE("The filter attenuates a single-sample spike", "[filter]")
{
    ExpFilter f(3U);
    f.update(1000U);

    const uint16_t after_spike = f.update(4095U);

    /* A lone spike must move the output far less than the raw jump. */
    REQUIRE(after_spike > 1000U);
    REQUIRE(after_spike < 1500U);
}

TEST_CASE("shift 0 passes samples straight through", "[filter]")
{
    ExpFilter f(0U);

    REQUIRE(f.update(1234U) == 1234U);
    REQUIRE(f.update(4095U) == 4095U);
    REQUIRE(f.update(0U) == 0U);
}

TEST_CASE("A larger shift smooths more slowly", "[filter]")
{
    ExpFilter fast(1U);
    ExpFilter slow(6U);
    fast.update(0U);
    slow.update(0U);

    for (int i = 0; i < 5; ++i) {
        fast.update(4000U);
        slow.update(4000U);
    }
    REQUIRE(fast.value() > slow.value());
}

TEST_CASE("reset() re-seeds from the next sample", "[filter]")
{
    ExpFilter f(3U);
    f.update(4000U);

    f.reset();
    REQUIRE(f.update(100U) == 100U);
}

TEST_CASE("Hysteresis holds between the thresholds", "[filter]")
{
    Hysteresis h(300U, 700U);

    REQUIRE(h.update(500U) == false);   // starts low, 500 is below `high`
    REQUIRE(h.update(699U) == false);
    REQUIRE(h.update(700U) == true);    // crosses `high`
    REQUIRE(h.update(400U) == true);    // above `low` - holds
    REQUIRE(h.update(301U) == true);
    REQUIRE(h.update(300U) == false);   // crosses `low`
    REQUIRE(h.update(699U) == false);   // needs `high` again to re-arm
}

TEST_CASE("Hysteresis suppresses chatter on a noisy threshold", "[filter]")
{
    Hysteresis h(300U, 700U);
    h.update(800U);                      // latch on

    /* Noise around the midpoint must not produce a single transition. */
    int transitions = 0;
    bool prev = h.state();
    const uint16_t noise[] = { 480U, 520U, 495U, 505U, 460U, 540U };
    for (uint16_t v : noise) {
        const bool now = h.update(v);
        if (now != prev) { ++transitions; prev = now; }
    }
    REQUIRE(transitions == 0);
}

TEST_CASE("Hysteresis honours its initial state", "[filter]")
{
    Hysteresis h(300U, 700U, true);
    REQUIRE(h.state() == true);
    REQUIRE(h.update(500U) == true);   // holds until it drops below `low`
}
