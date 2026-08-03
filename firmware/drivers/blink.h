/**
 * @file    blink.h
 * @brief   Non-blocking lamp pattern generator - pure logic, no HAL.
 *
 * Indicators must flash while everything else keeps running, so blinking is
 * driven by elapsed time rather than HAL_Delay. Once FreeRTOS arrives in
 * Phase 4 the same object is ticked from a task instead of the super-loop.
 *
 * SRS-IND-004: a lamp starts in the ON phase when the indicator is activated,
 * so the driver never sees a dark first blink.
 */
#ifndef BCM_BLINK_H
#define BCM_BLINK_H

#include <stdint.h>

namespace bcm {
namespace drivers {

class Blinker {
public:
    enum class Mode : uint8_t { Off, On, Blink };

    Blinker()
        : mode_(Mode::Off),
          on_ms_(0U),
          off_ms_(0U),
          elapsed_(0U),
          output_(false)
    {
    }

    void set_off()
    {
        mode_    = Mode::Off;
        output_  = false;
        elapsed_ = 0U;
    }

    void set_on()
    {
        mode_    = Mode::On;
        output_  = true;
        elapsed_ = 0U;
    }

    /**
     * @brief Start blinking.
     * @param on_ms    time lit.
     * @param off_ms   time dark.
     * @param start_on begin in the ON phase (SRS-IND-004). Default true.
     *
     * Restarting with identical parameters does not reset the phase, so
     * holding an indicator on does not stall it permanently lit.
     */
    void set_blink(uint16_t on_ms, uint16_t off_ms, bool start_on = true)
    {
        if (mode_ == Mode::Blink && on_ms == on_ms_ && off_ms == off_ms_) {
            return;
        }
        mode_    = Mode::Blink;
        on_ms_   = on_ms;
        off_ms_  = off_ms;
        output_  = start_on;
        elapsed_ = 0U;
    }

    /**
     * @brief Advance the pattern.
     * @param dt_ms milliseconds since the previous call.
     * @return the level the lamp should be driven to now.
     */
    bool update(uint16_t dt_ms)
    {
        if (mode_ != Mode::Blink) { return output_; }

        const uint32_t sum = static_cast<uint32_t>(elapsed_) + dt_ms;
        elapsed_ = (sum > 0xFFFFU) ? 0xFFFFU : static_cast<uint16_t>(sum);

        /* while() not if() so a long dt cannot leave the phase stale. */
        for (;;) {
            const uint16_t limit = output_ ? on_ms_ : off_ms_;
            if (limit == 0U || elapsed_ < limit) { break; }
            elapsed_ = static_cast<uint16_t>(elapsed_ - limit);
            output_  = !output_;
        }
        return output_;
    }

    bool output() const { return output_; }
    Mode mode() const { return mode_; }

private:
    Mode     mode_;
    uint16_t on_ms_;
    uint16_t off_ms_;
    uint16_t elapsed_;
    bool     output_;
};

}  // namespace drivers
}  // namespace bcm

#endif /* BCM_BLINK_H */
