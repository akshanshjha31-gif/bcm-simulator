/**
 * @file    horn_mgr.h
 * @brief   Horn / buzzer control with chirps and rate limiting - pure logic.
 *
 * Traces SRS-HORN-001 (horn follows its input) and SRS-HORN-002 (lock and
 * unlock chirps). The rate limit exists so a stuck input cannot leave the
 * buzzer energised indefinitely.
 */
#ifndef BCM_HORN_MGR_H
#define BCM_HORN_MGR_H

#include "bcm_config.h"
#include "soft_timer.h"

namespace bcm {
namespace services {

class HornMgr {
public:
    explicit HornMgr(const Config& config)
        : cfg_(config), chirps_left_(0U), chirp_on_(false),
          continuous_(false), output_(false), on_time_ms_(0U)
    {
    }

    /// Sound the horn while asserted (SRS-HORN-001).
    void set_continuous(bool on)
    {
        if (!on) { on_time_ms_ = 0U; }
        continuous_ = on;
    }

    /// Queue @p count short chirps (lock/unlock feedback).
    void chirp(uint8_t count)
    {
        if (count == 0U) { return; }
        chirps_left_ = count;
        chirp_on_    = true;
        timer_.start(cfg_.chirp_ms);
    }

    /// Advance timers and compute the output level.
    bool update(uint32_t dt_ms)
    {
        /* Chirps take priority - they are short and carry meaning. */
        if (chirps_left_ > 0U) {
            if (timer_.update(dt_ms)) {
                if (chirp_on_) {
                    chirp_on_ = false;
                    timer_.start(cfg_.chirp_gap_ms);
                    --chirps_left_;
                }
                else {
                    chirp_on_ = true;
                    timer_.start(cfg_.chirp_ms);
                }
            }
            output_ = (chirps_left_ > 0U) && chirp_on_;
            return output_;
        }

        /* Rate limit: a stuck input must not burn out the buzzer. */
        if (continuous_) {
            on_time_ms_ += dt_ms;
            output_ = (on_time_ms_ < cfg_.horn_max_on_ms);
        }
        else {
            output_ = false;
        }
        return output_;
    }

    bool output() const { return output_; }
    bool chirping() const { return chirps_left_ > 0U; }

    void reset()
    {
        chirps_left_ = 0U;
        chirp_on_    = false;
        continuous_  = false;
        output_      = false;
        on_time_ms_  = 0U;
        timer_.stop();
    }

private:
    const Config&      cfg_;
    drivers::SoftTimer timer_;
    uint8_t            chirps_left_;
    bool               chirp_on_;
    bool               continuous_;
    bool               output_;
    uint32_t           on_time_ms_;
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_HORN_MGR_H */
