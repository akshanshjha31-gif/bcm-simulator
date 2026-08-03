/**
 * @file    diag_mgr.cpp
 * @brief   Power-on self test and health aggregation.
 */
#include "diag_mgr.h"

namespace bcm {
namespace services {
namespace {
const uint8_t kCount = static_cast<uint8_t>(Subsystem::Count);
}  // namespace

DiagMgr::DiagMgr() : post_done_(false)
{
    reset();
}

void DiagMgr::reset()
{
    for (uint8_t i = 0U; i < kCount; ++i) {
        health_[i]      = Health::Unknown;
        post_tested_[i] = false;
        post_ok_[i]     = false;
    }
    post_done_ = false;
}

void DiagMgr::post_result(Subsystem sys, bool passed)
{
    const uint8_t i = static_cast<uint8_t>(sys);
    if (i >= kCount) { return; }

    post_tested_[i] = true;
    post_ok_[i]     = passed;

    /* A POST failure seeds the running health too - a subsystem that failed
     * at startup is not healthy just because nobody has reported on it since. */
    health_[i] = passed ? Health::Ok : Health::Failed;
}

bool DiagMgr::post_passed() const
{
    if (!post_done_) { return false; }

    for (uint8_t i = 0U; i < kCount; ++i) {
        /* An untested subsystem is NOT a pass. Treating "no result" as
         * success is how self-tests come to mean nothing. */
        if (!post_tested_[i] || !post_ok_[i]) { return false; }
    }
    return true;
}

uint8_t DiagMgr::post_failures() const
{
    uint8_t mask = 0U;
    for (uint8_t i = 0U; i < kCount; ++i) {
        if (!post_tested_[i] || !post_ok_[i]) {
            mask = static_cast<uint8_t>(mask | (1U << i));
        }
    }
    return mask;
}

void DiagMgr::set_health(Subsystem sys, Health state)
{
    const uint8_t i = static_cast<uint8_t>(sys);
    if (i < kCount) { health_[i] = state; }
}

Health DiagMgr::health(Subsystem sys) const
{
    const uint8_t i = static_cast<uint8_t>(sys);
    return (i < kCount) ? health_[i] : Health::Unknown;
}

Health DiagMgr::overall() const
{
    Health worst = Health::Ok;
    for (uint8_t i = 0U; i < kCount; ++i) {
        switch (health_[i]) {
        case Health::Failed:
            return Health::Failed;             /* cannot get worse */
        case Health::Degraded:
            worst = Health::Degraded;
            break;
        case Health::Unknown:
            if (worst == Health::Ok) { worst = Health::Unknown; }
            break;
        case Health::Ok:
        default:
            break;
        }
    }
    return worst;
}

}  // namespace services
}  // namespace bcm
