/**
 * @file    filter.h
 * @brief   Integer low-pass filter and hysteresis - pure logic, no HAL.
 *
 * ADC readings from the battery divider and the ambient sensor are noisy. A
 * raw sample crossing a threshold would chatter, so both are smoothed and
 * then compared with hysteresis (SRS-SENS-001, SRS-LIGHT-003).
 *
 * Fixed-point only - no float on the target (determinism, code size).
 */
#ifndef BCM_FILTER_H
#define BCM_FILTER_H

#include <stdint.h>

namespace bcm {
namespace drivers {

/**
 * @brief Exponential moving average, y += (x - y) / 2^shift.
 *
 * shift=0 passes samples through untouched; larger shift = smoother/slower.
 * The accumulator keeps the fractional bits so slow drifts are not lost to
 * truncation.
 */
class ExpFilter {
public:
    explicit ExpFilter(uint8_t shift = 3U)
        : shift_(shift), acc_(0U), primed_(false)
    {
    }

    uint16_t update(uint16_t sample)
    {
        if (!primed_) {
            /* Seed with the first sample so startup does not ramp from zero. */
            acc_    = static_cast<uint32_t>(sample) << shift_;
            primed_ = true;
        }
        else {
            acc_ = acc_ - (acc_ >> shift_) + sample;
        }
        return value();
    }

    uint16_t value() const
    {
        return static_cast<uint16_t>(acc_ >> shift_);
    }

    void reset()
    {
        acc_    = 0U;
        primed_ = false;
    }

private:
    uint8_t  shift_;
    uint32_t acc_;
    bool     primed_;
};

/**
 * @brief Schmitt trigger over a scalar.
 *
 * Output goes true once the input rises above `high`, and false only once it
 * falls below `low`. Between the two it holds, which is what stops the
 * auto-headlight flickering at dusk.
 */
class Hysteresis {
public:
    Hysteresis(uint16_t low, uint16_t high, bool initial = false)
        : low_(low), high_(high), state_(initial)
    {
    }

    bool update(uint16_t value)
    {
        if (!state_ && value >= high_)     { state_ = true; }
        else if (state_ && value <= low_)  { state_ = false; }
        return state_;
    }

    bool state() const { return state_; }

private:
    uint16_t low_;
    uint16_t high_;
    bool     state_;
};

}  // namespace drivers
}  // namespace bcm

#endif /* BCM_FILTER_H */
