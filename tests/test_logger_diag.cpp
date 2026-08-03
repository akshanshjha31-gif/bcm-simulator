/**
 * @file    test_logger_diag.cpp
 * @brief   Unit tests for the event logger and the diagnostics manager.
 *
 * Traces SRS-LOG-001, SRS-DIAG-001 (POST) and SRS-DIAG-003 (health).
 */
#include "catch2/catch.hpp"
#include "diag_mgr.h"
#include "logger.h"

using bcm::services::DiagMgr;
using bcm::services::Health;
using bcm::services::LogEvent;
using bcm::services::Logger;
using bcm::services::LogRecord;
using bcm::services::Subsystem;

/* ---- Logger ------------------------------------------------------------- */

TEST_CASE("A new log is empty", "[logger]")
{
    Logger l;
    REQUIRE(l.empty());
    REQUIRE(l.size() == 0U);
    REQUIRE(l.dropped() == 0U);

    LogRecord r;
    REQUIRE_FALSE(l.pop(r));
}

TEST_CASE("Records come back oldest first", "[logger]")
{
    Logger l;
    l.log(100U, LogEvent::IgnitionOn);
    l.log(200U, LogEvent::BrakeApplied);
    l.log(300U, LogEvent::DoorLocked);

    LogRecord r;
    REQUIRE(l.pop(r));
    REQUIRE(r.event == LogEvent::IgnitionOn);
    REQUIRE(r.timestamp_ms == 100U);

    REQUIRE(l.pop(r));
    REQUIRE(r.event == LogEvent::BrakeApplied);

    REQUIRE(l.pop(r));
    REQUIRE(r.event == LogEvent::DoorLocked);
    REQUIRE(l.empty());
}

TEST_CASE("The argument byte is preserved", "[logger]")
{
    Logger l;
    l.log(50U, LogEvent::LightMode, 3U);

    LogRecord r;
    REQUIRE(l.pop(r));
    REQUIRE(r.arg == 3U);
}

TEST_CASE("LogEvent::None is ignored", "[logger]")
{
    Logger l;
    l.log(10U, LogEvent::None);
    REQUIRE(l.empty());
}

TEST_CASE("Overflow drops the OLDEST record, not the newest", "[logger]")
{
    /* During a fault the most recent events explain it, so the tail is what
     * must survive. */
    Logger l;
    for (uint8_t i = 0U; i < Logger::kCapacity; ++i) {
        l.log(i, LogEvent::BrakeApplied, i);
    }
    REQUIRE(l.size() == Logger::kCapacity);
    REQUIRE(l.dropped() == 0U);

    l.log(999U, LogEvent::CommsLost, 0xAAU);
    REQUIRE(l.dropped() == 1U);
    REQUIRE(l.size() == Logger::kCapacity);

    /* The oldest (arg 0) is gone; the next oldest is arg 1. */
    LogRecord r;
    REQUIRE(l.pop(r));
    REQUIRE(r.arg == 1U);
}

TEST_CASE("The newest record survives a full buffer", "[logger]")
{
    Logger l;
    for (uint8_t i = 0U; i < Logger::kCapacity + 5U; ++i) {
        l.log(i, LogEvent::BrakeApplied, i);
    }

    /* Drain to the end and check the last one written is present. */
    LogRecord r;
    LogRecord last;
    while (l.pop(r)) { last = r; }
    REQUIRE(last.arg == Logger::kCapacity + 4U);
}

TEST_CASE("Dropped records are counted, never silent", "[logger]")
{
    Logger l;
    for (uint16_t i = 0U; i < Logger::kCapacity + 10U; ++i) {
        l.log(i, LogEvent::CommsLost);
    }
    REQUIRE(l.dropped() == 10U);
}

TEST_CASE("clear() empties the log and its counters", "[logger]")
{
    Logger l;
    for (uint16_t i = 0U; i < Logger::kCapacity + 3U; ++i) {
        l.log(i, LogEvent::HazardOn);
    }
    l.clear();

    REQUIRE(l.empty());
    REQUIRE(l.dropped() == 0U);
}

TEST_CASE("Every event has a readable name", "[logger]")
{
    for (uint8_t i = 1U; i < static_cast<uint8_t>(LogEvent::Count); ++i) {
        const char* n = Logger::name(static_cast<LogEvent>(i));
        REQUIRE(n != 0);
        REQUIRE(n[0] != '\0');
        REQUIRE(n[0] != '?');       // no unnamed events
    }
}

/* ---- Diagnostics -------------------------------------------------------- */

TEST_CASE("POST has not passed before it has run", "[diag]")
{
    DiagMgr d;
    REQUIRE_FALSE(d.post_done());
    REQUIRE_FALSE(d.post_passed());
}

TEST_CASE("POST passes only when every subsystem passed", "[diag]")
{
    DiagMgr d;
    for (uint8_t i = 0U; i < static_cast<uint8_t>(Subsystem::Count); ++i) {
        d.post_result(static_cast<Subsystem>(i), true);
    }
    d.post_complete();

    REQUIRE(d.post_passed());
    REQUIRE(d.post_failures() == 0U);
}

TEST_CASE("An untested subsystem is NOT a pass", "[diag][safety]")
{
    /* Treating "no result" as success is how a self-test comes to mean
     * nothing at all. */
    DiagMgr d;
    d.post_result(Subsystem::Clock, true);
    d.post_result(Subsystem::Gpio, true);
    d.post_complete();

    REQUIRE_FALSE(d.post_passed());
    REQUIRE(d.post_failures() != 0U);
}

TEST_CASE("A single POST failure fails the whole test", "[diag]")
{
    DiagMgr d;
    for (uint8_t i = 0U; i < static_cast<uint8_t>(Subsystem::Count); ++i) {
        d.post_result(static_cast<Subsystem>(i), true);
    }
    d.post_result(Subsystem::Adc, false);
    d.post_complete();

    REQUIRE_FALSE(d.post_passed());
    REQUIRE((d.post_failures() & (1U << static_cast<uint8_t>(Subsystem::Adc))) != 0U);
}

TEST_CASE("A POST failure seeds the running health", "[diag]")
{
    /* A subsystem that failed at startup is not healthy merely because
     * nobody has reported on it since. */
    DiagMgr d;
    d.post_result(Subsystem::Uart, false);
    REQUIRE(d.health(Subsystem::Uart) == Health::Failed);
    REQUIRE_FALSE(d.healthy());
}

TEST_CASE("Overall health is the worst subsystem", "[diag]")
{
    DiagMgr d;
    for (uint8_t i = 0U; i < static_cast<uint8_t>(Subsystem::Count); ++i) {
        d.set_health(static_cast<Subsystem>(i), Health::Ok);
    }
    REQUIRE(d.overall() == Health::Ok);

    d.set_health(Subsystem::Adc, Health::Degraded);
    REQUIRE(d.overall() == Health::Degraded);

    d.set_health(Subsystem::Uart, Health::Failed);
    REQUIRE(d.overall() == Health::Failed);
}

TEST_CASE("Failed dominates regardless of ordering", "[diag]")
{
    DiagMgr d;
    d.set_health(Subsystem::Clock, Health::Failed);
    for (uint8_t i = 1U; i < static_cast<uint8_t>(Subsystem::Count); ++i) {
        d.set_health(static_cast<Subsystem>(i), Health::Ok);
    }
    REQUIRE(d.overall() == Health::Failed);
    REQUIRE_FALSE(d.healthy());
}

TEST_CASE("Unknown health is not treated as OK", "[diag]")
{
    /* The watchdog supervisor gates on this, so an unreported subsystem must
     * not read as healthy-by-default. */
    DiagMgr d;
    d.set_health(Subsystem::Clock, Health::Ok);
    REQUIRE(d.overall() == Health::Unknown);
}

TEST_CASE("healthy() is true only when nothing has Failed", "[diag]")
{
    DiagMgr d;
    for (uint8_t i = 0U; i < static_cast<uint8_t>(Subsystem::Count); ++i) {
        d.set_health(static_cast<Subsystem>(i), Health::Degraded);
    }
    REQUIRE(d.healthy());          // degraded still runs

    d.set_health(Subsystem::Gpio, Health::Failed);
    REQUIRE_FALSE(d.healthy());
}

TEST_CASE("reset() clears POST and health", "[diag]")
{
    DiagMgr d;
    d.post_result(Subsystem::Adc, false);
    d.post_complete();

    d.reset();
    REQUIRE_FALSE(d.post_done());
    REQUIRE(d.health(Subsystem::Adc) == Health::Unknown);
}
