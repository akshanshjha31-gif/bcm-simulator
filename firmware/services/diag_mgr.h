/**
 * @file    diag_mgr.h
 * @brief   Power-on self test and health aggregation - pure logic, no HAL.
 *
 * POST runs once at startup and reports per-subsystem results. Health is the
 * running picture afterwards: each registered subsystem checks in, and the
 * aggregate is what the watchdog supervisor and the diagnostic host consult.
 *
 * Traces SRS-DIAG-001 (POST), SRS-DIAG-003 (health), SRS-SAFETY-006.
 */
#ifndef BCM_DIAG_MGR_H
#define BCM_DIAG_MGR_H

#include <stdint.h>

namespace bcm {
namespace services {

/// Subsystems covered by POST and by the health check.
enum class Subsystem : uint8_t {
    Clock = 0,
    Gpio,
    Adc,
    Uart,
    Config,
    Count
};

enum class Health : uint8_t {
    Unknown = 0,
    Ok,
    Degraded,
    Failed
};

class DiagMgr {
public:
    DiagMgr();

    /* ---- Power-on self test ---------------------------------------------- */

    /// Record one POST result. Call once per subsystem during startup.
    void post_result(Subsystem sys, bool passed);

    /// Mark POST finished; after this, post_passed() is meaningful.
    void post_complete() { post_done_ = true; }

    bool post_done() const { return post_done_; }

    /// True only if every subsystem was tested and passed.
    bool post_passed() const;

    /// Bitmap of subsystems that failed POST, for the diagnostic link.
    uint8_t post_failures() const;

    /* ---- Running health -------------------------------------------------- */

    void set_health(Subsystem sys, Health state);
    Health health(Subsystem sys) const;

    /// Worst health across all subsystems.
    Health overall() const;

    /// True when nothing is Failed - what the supervisor gates on.
    bool healthy() const { return overall() != Health::Failed; }

    void reset();

private:
    Health  health_[static_cast<uint8_t>(Subsystem::Count)];
    bool    post_tested_[static_cast<uint8_t>(Subsystem::Count)];
    bool    post_ok_[static_cast<uint8_t>(Subsystem::Count)];
    bool    post_done_;
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_DIAG_MGR_H */
