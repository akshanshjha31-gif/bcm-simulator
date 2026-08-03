/**
 * @file    debounce.h
 * @brief   Switch debouncing - pure logic, no HAL.
 *
 * Mechanical contacts bounce for 5-20 ms. Feeding raw samples straight to a
 * state machine turns one press into several transitions, which would make
 * the Indicator FSM toggle unpredictably.
 *
 * A raw level must hold steady for `stable_ms` before it is accepted. Any
 * glitch restarts the count, so noise never propagates upward.
 *
 * HAL-free by design so it compiles and is unit-tested on the host
 * (SRS-MNT-002). Debounce window feeds the 50 ms switch-to-lamp budget
 * (SRS-PERF-001).
 */
#ifndef BCM_DEBOUNCE_H
#define BCM_DEBOUNCE_H

#include <stdint.h>

namespace bcm {
namespace drivers {

class Debouncer {
public:
    /// @param stable_ms how long a level must hold before it is accepted.
    /// @param initial   assumed starting level (released, for active-high).
    explicit Debouncer(uint16_t stable_ms = 20U, bool initial = false)
        : stable_ms_(stable_ms),
          elapsed_(0U),
          state_(initial),
          candidate_(initial),
          rising_(false),
          falling_(false)
    {
    }

    /**
     * @brief Feed one raw sample.
     * @param raw    level read from the pin right now.
     * @param dt_ms  milliseconds since the previous call.
     * @return true if the debounced state changed on this call.
     */
    bool update(bool raw, uint16_t dt_ms)
    {
        rising_  = false;
        falling_ = false;

        if (raw != candidate_) {
            /* Level moved - restart the stability window. */
            candidate_ = raw;
            elapsed_   = 0U;
            return false;
        }

        if (candidate_ == state_) {
            /* Already settled here; nothing to time. */
            elapsed_ = 0U;
            return false;
        }

        /* Saturating add - a long dt must never wrap and lose the change. */
        const uint32_t sum = static_cast<uint32_t>(elapsed_) + dt_ms;
        elapsed_ = (sum > 0xFFFFU) ? 0xFFFFU : static_cast<uint16_t>(sum);

        if (elapsed_ >= stable_ms_) {
            state_   = candidate_;
            elapsed_ = 0U;
            rising_  = state_;
            falling_ = !state_;
            return true;
        }
        return false;
    }

    /// Debounced level.
    bool state() const { return state_; }

    /// True only on the update() call that saw a 0->1 transition.
    bool rising() const { return rising_; }

    /// True only on the update() call that saw a 1->0 transition.
    bool falling() const { return falling_; }

    void reset(bool level)
    {
        state_     = level;
        candidate_ = level;
        elapsed_   = 0U;
        rising_    = false;
        falling_   = false;
    }

private:
    uint16_t stable_ms_;
    uint16_t elapsed_;
    bool     state_;
    bool     candidate_;
    bool     rising_;
    bool     falling_;
};

}  // namespace drivers
}  // namespace bcm

#endif /* BCM_DEBOUNCE_H */
