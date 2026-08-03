/**
 * @file    logger.h
 * @brief   Timestamped event log - pure logic, no HAL.
 *
 * A fixed-size ring of records. When it fills, the OLDEST record is dropped
 * rather than the newest: during a fault the most recent events are the ones
 * that explain it, so losing history is preferable to losing the tail.
 * Dropped records are counted so the loss is never silent.
 *
 * Traces SRS-LOG-001.
 */
#ifndef BCM_LOGGER_H
#define BCM_LOGGER_H

#include <stdint.h>

namespace bcm {
namespace services {

/// Event identifiers. Kept compact - one byte on the wire.
enum class LogEvent : uint8_t {
    None = 0,
    Startup,
    PostPassed,
    PostFailed,
    IgnitionOn,
    IgnitionOff,
    LightMode,          ///< arg = new LightState
    IndicatorLeft,
    IndicatorRight,
    HazardOn,
    HazardOff,
    BrakeApplied,
    BrakeReleased,
    DoorLocked,
    DoorUnlocked,
    LockRefused,        ///< door open (SRS-DOOR-002)
    AutoLocked,
    BatteryLow,
    BatteryOk,
    CommsLost,
    CommsRestored,
    WatchdogReset,
    LoadShed,
    Count
};

struct LogRecord {
    uint32_t timestamp_ms;
    LogEvent event;
    uint8_t  arg;

    LogRecord() : timestamp_ms(0U), event(LogEvent::None), arg(0U) {}
};

class Logger {
public:
    enum : uint8_t { kCapacity = 32U };

    Logger() : head_(0U), count_(0U), dropped_(0U) {}

    /// Append a record, overwriting the oldest if full.
    void log(uint32_t timestamp_ms, LogEvent event, uint8_t arg = 0U);

    /// Pop the oldest record. False when empty.
    bool pop(LogRecord& out);

    bool     empty() const { return count_ == 0U; }
    uint8_t  size() const { return count_; }
    uint16_t dropped() const { return dropped_; }

    void clear();

    /// Human-readable name, for the diagnostic UART.
    static const char* name(LogEvent event);

private:
    LogRecord records_[kCapacity];
    uint8_t   head_;      ///< next write slot
    uint8_t   count_;
    uint16_t  dropped_;
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_LOGGER_H */
