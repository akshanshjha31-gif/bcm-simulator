/**
 * @file    fault_mgr.cpp
 * @brief   Diagnostic Trouble Code store.
 */
#include "fault_mgr.h"

namespace bcm {
namespace services {

int FaultMgr::find(Dtc code) const
{
    for (uint8_t i = 0U; i < count_; ++i) {
        if (entries_[i].code == code) { return static_cast<int>(i); }
    }
    return -1;
}

void FaultMgr::set(Dtc code)
{
    if (code == Dtc::None) { return; }

    const int index = find(code);
    if (index >= 0) {
        Entry& e = entries_[index];
        /* Saturate rather than wrap - a wrapped counter reads as "healthy". */
        if (e.occurrences < 0xFFFFU) { ++e.occurrences; }
        e.active = true;
        return;
    }

    if (count_ >= kMaxDtc) { return; }   /* full: keep the oldest faults */

    entries_[count_].code        = code;
    entries_[count_].occurrences = 1U;
    entries_[count_].active      = true;
    ++count_;
}

void FaultMgr::clear(Dtc code)
{
    const int index = find(code);
    if (index >= 0) { entries_[index].active = false; }
}

void FaultMgr::clear_all()
{
    for (uint8_t i = 0U; i < kMaxDtc; ++i) { entries_[i] = Entry(); }
    count_ = 0U;
}

bool FaultMgr::is_active(Dtc code) const
{
    const int index = find(code);
    return (index >= 0) && entries_[index].active;
}

uint8_t FaultMgr::active_count() const
{
    uint8_t n = 0U;
    for (uint8_t i = 0U; i < count_; ++i) {
        if (entries_[i].active) { ++n; }
    }
    return n;
}

const FaultMgr::Entry& FaultMgr::at(uint8_t index) const
{
    static const Entry empty;
    return (index < count_) ? entries_[index] : empty;
}

uint8_t FaultMgr::active_codes(uint8_t* out, uint8_t capacity) const
{
    if (out == 0) { return 0U; }

    uint8_t n = 0U;
    for (uint8_t i = 0U; i < count_ && n < capacity; ++i) {
        if (entries_[i].active) {
            out[n++] = static_cast<uint8_t>(entries_[i].code);
        }
    }
    return n;
}

}  // namespace services
}  // namespace bcm
