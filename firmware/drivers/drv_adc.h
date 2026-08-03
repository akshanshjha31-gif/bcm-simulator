/**
 * @file    drv_adc.h
 * @brief   ADC sampling with filtering for battery voltage and ambient light.
 *
 * Adc owns the peripheral; AnalogInput binds one channel to an ExpFilter so
 * consumers see a smoothed value rather than a noisy raw sample
 * (SRS-SENS-001).
 */
#ifndef BCM_DRV_ADC_H
#define BCM_DRV_ADC_H

#include "stm32f1xx_hal.h"
#include "filter.h"
#include <stdint.h>

namespace bcm {
namespace drivers {

class Adc {
public:
    /// Bring up ADC1 and run self-calibration. Safe to call once.
    static bool init();

    /// Blocking single conversion. Returns 0 on error.
    static uint16_t read_raw(uint32_t channel);

    static bool ready() { return initialised_; }

    /// 12-bit full scale.
    static const uint16_t kFullScale = 4095U;

private:
    static ADC_HandleTypeDef handle_;
    static bool              initialised_;
};

/**
 * @brief One filtered analog channel.
 */
class AnalogInput {
public:
    AnalogInput() : channel_(0U), filter_(3U) {}

    explicit AnalogInput(uint32_t channel, uint8_t filter_shift = 3U)
        : channel_(channel), filter_(filter_shift)
    {
    }

    /// Convert once and fold the result into the filter.
    uint16_t sample() { return filter_.update(Adc::read_raw(channel_)); }

    /// Most recent filtered value, without converting again.
    uint16_t value() const { return filter_.value(); }

    /// Filtered value scaled to 0..1000 (per mille of full scale).
    uint16_t permille() const
    {
        return static_cast<uint16_t>(
            (static_cast<uint32_t>(filter_.value()) * 1000U) / Adc::kFullScale);
    }

    void reset() { filter_.reset(); }

private:
    uint32_t  channel_;
    ExpFilter filter_;
};

}  // namespace drivers
}  // namespace bcm

#endif /* BCM_DRV_ADC_H */
