/**
 * @file    soft_timer.h
 * @brief   Software one-shot / periodic timer - pure logic, no HAL.
 *
 * Used for auto-lock delays, welcome-lighting durations and comm timeouts.
 * Driven by elapsed milliseconds rather than an absolute tick, so the same
 * object works on target, in unit tests and in the SIL.
 */
#ifndef BCM_SOFT_TIMER_H
#define BCM_SOFT_TIMER_H

#include <stdint.h>

namespace bcm {
namespace drivers {

class SoftTimer {
public:
    SoftTimer()
        : period_(0U), remaining_(0U), periodic_(false), running_(false)
    {
    }

    void start(uint32_t period_ms, bool periodic = false)
    {
        period_    = period_ms;
        remaining_ = period_ms;
        periodic_  = periodic;
        running_   = (period_ms > 0U);
    }

    void stop() { running_ = false; }

    /**
     * @brief Advance the timer.
     * @return true exactly once per expiry. A periodic timer reloads itself;
     *         a one-shot stops.
     */
    bool update(uint32_t dt_ms)
    {
        if (!running_) { return false; }

        if (dt_ms >= remaining_) {
            if (periodic_ && period_ > 0U) {
                /* Carry the overshoot so periods do not drift. */
                const uint32_t overshoot = dt_ms - remaining_;
                remaining_ = period_ - (overshoot % period_);
            }
            else {
                remaining_ = 0U;
                running_   = false;
            }
            return true;
        }

        remaining_ -= dt_ms;
        return false;
    }

    bool     running() const { return running_; }
    uint32_t remaining() const { return remaining_; }

private:
    uint32_t period_;
    uint32_t remaining_;
    bool     periodic_;
    bool     running_;
};

}  // namespace drivers
}  // namespace bcm

#endif /* BCM_SOFT_TIMER_H */
