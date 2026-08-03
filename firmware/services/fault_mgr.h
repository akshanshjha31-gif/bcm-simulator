/**
 * @file    fault_mgr.h
 * @brief   Diagnostic Trouble Code store - pure logic, no HAL.
 *
 * Fixed-size store, no allocation. A DTC that is set repeatedly is recorded
 * once with an occurrence count, so a chattering fault cannot flush the
 * others out of the table.
 *
 * Traces SRS-DIAG-002, SRS-LOG-001.
 */
#ifndef BCM_FAULT_MGR_H
#define BCM_FAULT_MGR_H

#include <stdint.h>

namespace bcm {
namespace services {

/// Diagnostic trouble codes. Values are part of BCM-ICD-001 (GET_DTC).
enum class Dtc : uint8_t {
    None            = 0x00U,
    BatteryLow      = 0x01U,
    CommsLost       = 0x02U,
    LockRefusedDoorOpen = 0x03U,
    WatchdogReset   = 0x04U,
    UartOverrun     = 0x05U,
    PostFailed      = 0x06U,
    Count           = 0x07U
};

class FaultMgr {
public:
    /* An enum, not `static const`: prvalues cannot be ODR-used, so binding
     * this to a const reference (as the tests do) needs no out-of-class
     * definition. Same reasoning as Crc8's constants. */
    enum : uint8_t { kMaxDtc = 8U };

    struct Entry {
        Dtc      code;
        uint16_t occurrences;
        bool     active;

        Entry() : code(Dtc::None), occurrences(0U), active(false) {}
    };

    FaultMgr() : count_(0U) {}

    /// Record a fault. Repeats increment the counter rather than duplicating.
    void set(Dtc code);

    /// Mark a fault no longer present. History is retained until cleared.
    void clear(Dtc code);

    /// Wipe the store entirely (CLEAR_DTC).
    void clear_all();

    bool    is_active(Dtc code) const;
    uint8_t active_count() const;

    /// Stored entries, including inactive history.
    uint8_t       count() const { return count_; }
    const Entry&  at(uint8_t index) const;

    /// Serialise active codes into @p out, returning how many were written.
    uint8_t active_codes(uint8_t* out, uint8_t capacity) const;

private:
    Entry   entries_[kMaxDtc];
    uint8_t count_;

    int find(Dtc code) const;
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_FAULT_MGR_H */
