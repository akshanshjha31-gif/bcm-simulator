/**
 * @file    logger.cpp
 * @brief   Timestamped event log.
 */
#include "logger.h"

namespace bcm {
namespace services {

void Logger::log(uint32_t timestamp_ms, LogEvent event, uint8_t arg)
{
    if (event == LogEvent::None) { return; }

    records_[head_].timestamp_ms = timestamp_ms;
    records_[head_].event        = event;
    records_[head_].arg          = arg;

    head_ = static_cast<uint8_t>((head_ + 1U) % kCapacity);

    if (count_ < kCapacity) {
        ++count_;
    }
    else {
        /* Full: the write above overwrote the oldest record. Count it so the
         * loss is visible rather than silent. */
        if (dropped_ < 0xFFFFU) { ++dropped_; }
    }
}

bool Logger::pop(LogRecord& out)
{
    if (count_ == 0U) { return false; }

    /* Oldest record sits `count_` slots behind the write cursor. */
    const uint8_t tail =
        static_cast<uint8_t>((head_ + kCapacity - count_) % kCapacity);
    out = records_[tail];
    --count_;
    return true;
}

void Logger::clear()
{
    head_    = 0U;
    count_   = 0U;
    dropped_ = 0U;
}

const char* Logger::name(LogEvent event)
{
    switch (event) {
    case LogEvent::Startup:        return "Startup";
    case LogEvent::PostPassed:     return "POST passed";
    case LogEvent::PostFailed:     return "POST FAILED";
    case LogEvent::IgnitionOn:     return "Ignition ON";
    case LogEvent::IgnitionOff:    return "Ignition off";
    case LogEvent::LightMode:      return "Light mode";
    case LogEvent::IndicatorLeft:  return "Indicator left";
    case LogEvent::IndicatorRight: return "Indicator right";
    case LogEvent::HazardOn:       return "Hazard ON";
    case LogEvent::HazardOff:      return "Hazard off";
    case LogEvent::BrakeApplied:   return "Brake applied";
    case LogEvent::BrakeReleased:  return "Brake released";
    case LogEvent::DoorLocked:     return "Door locked";
    case LogEvent::DoorUnlocked:   return "Door unlocked";
    case LogEvent::LockRefused:    return "LOCK REFUSED (door open)";
    case LogEvent::AutoLocked:     return "Auto-locked";
    case LogEvent::BatteryLow:     return "Battery LOW";
    case LogEvent::BatteryOk:      return "Battery ok";
    case LogEvent::CommsLost:      return "Comms LOST";
    case LogEvent::CommsRestored:  return "Comms restored";
    case LogEvent::WatchdogReset:  return "Watchdog reset";
    case LogEvent::LoadShed:       return "Load shed";
    case LogEvent::None:
    default:                       return "?";
    }
}

}  // namespace services
}  // namespace bcm
