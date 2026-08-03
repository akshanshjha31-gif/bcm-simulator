/**
 * @file    test_soft_timer.cpp
 * @brief   Unit tests for drivers::SoftTimer.
 *
 * Backs auto-lock delays (SRS-DOOR-003), welcome lighting (SRS-DOOR-004) and
 * the communication timeout (SRS-SAFETY-007).
 */
#include "catch2/catch.hpp"
#include "soft_timer.h"

using bcm::drivers::SoftTimer;

TEST_CASE("A new timer is stopped", "[timer]")
{
    SoftTimer t;
    REQUIRE_FALSE(t.running());
    REQUIRE_FALSE(t.update(1000U));
}

TEST_CASE("A one-shot fires once and stops", "[timer]")
{
    SoftTimer t;
    t.start(100U);

    REQUIRE_FALSE(t.update(50U));
    REQUIRE(t.update(50U));          // expires exactly on the boundary
    REQUIRE_FALSE(t.running());
    REQUIRE_FALSE(t.update(500U));   // stays fired
}

TEST_CASE("A one-shot fires when overshot", "[timer]")
{
    SoftTimer t;
    t.start(100U);

    REQUIRE(t.update(250U));         // a long pass must not miss the expiry
    REQUIRE_FALSE(t.running());
}

TEST_CASE("A periodic timer reloads itself", "[timer]")
{
    SoftTimer t;
    t.start(100U, true);

    int fires = 0;
    for (int ms = 0; ms < 1000; ++ms) {
        if (t.update(1U)) { ++fires; }
    }
    REQUIRE(fires == 10);
    REQUIRE(t.running());
}

TEST_CASE("A periodic timer keeps phase across a long pass", "[timer]")
{
    SoftTimer t;
    t.start(100U, true);

    /* 250 ms covers two expiries; update() reports one, but the phase must
     * stay aligned so the next fire lands at 300 ms, not 350 ms. */
    REQUIRE(t.update(250U));
    REQUIRE(t.remaining() == 50U);

    REQUIRE(t.update(50U));
    REQUIRE(t.remaining() == 100U);
}

TEST_CASE("stop() halts a running timer", "[timer]")
{
    SoftTimer t;
    t.start(100U, true);
    t.update(50U);

    t.stop();
    REQUIRE_FALSE(t.running());
    REQUIRE_FALSE(t.update(1000U));
}

TEST_CASE("Restarting resets the remaining time", "[timer]")
{
    SoftTimer t;
    t.start(100U);
    t.update(90U);

    t.start(100U);
    REQUIRE(t.remaining() == 100U);
    REQUIRE_FALSE(t.update(90U));
}

TEST_CASE("A zero period never runs", "[timer]")
{
    SoftTimer t;
    t.start(0U);

    REQUIRE_FALSE(t.running());
    REQUIRE_FALSE(t.update(1U));
}

TEST_CASE("Auto-lock style delay behaves as specified", "[timer]")
{
    /* SRS-DOOR-003: auto-lock after a configurable delay. */
    SoftTimer auto_lock;
    auto_lock.start(3000U);

    bool locked = false;
    for (int ms = 0; ms < 2999; ++ms) {
        if (auto_lock.update(1U)) { locked = true; }
    }
    REQUIRE_FALSE(locked);

    REQUIRE(auto_lock.update(1U));
}
