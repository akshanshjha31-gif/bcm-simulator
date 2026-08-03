/**
 * @file    sil_runner.h
 * @brief   Software-in-the-Loop fixture: a virtual vehicle around BcmCore.
 *
 * Holds virtual switch positions and sensor values, advances simulated time
 * in control-cycle steps, and records assertions. The object under test is
 * services::BcmCore - the same translation unit the firmware links - so a
 * passing scenario exercises the shipped logic rather than a model of it.
 *
 * Time is simulated, not slept: a thirty-second auto-lock scenario runs in
 * microseconds, which is what makes a large scenario suite practical in CI.
 */
#ifndef BCM_SIL_RUNNER_H
#define BCM_SIL_RUNNER_H

#include "bcm_config.h"
#include "bcm_core.h"
#include "bcm_types.h"

#include <string>
#include <vector>

namespace bcm {
namespace sil {

/// One recorded check within a scenario.
struct CheckResult {
    std::string description;
    std::string detail;      ///< populated only on failure
    uint32_t    at_ms;
    bool        passed;
};

struct ScenarioResult {
    std::string              name;
    std::string              requirement;   ///< SRS trace
    std::vector<CheckResult> checks;

    bool     passed() const;
    unsigned failures() const;
};

/**
 * @brief The virtual vehicle.
 */
class SilRunner {
public:
    explicit SilRunner(const services::Config& config);

    /* ---- Driver actions -------------------------------------------------- */

    void press(const char* control);
    void release(const char* control);
    /// Press and release, holding for @p hold_ms of simulated time.
    void tap(const char* control, uint32_t hold_ms = 60U);

    void set_reverse_gear(bool engaged) { inputs_.reverse_gear = engaged; }
    void set_door_open(bool open) { inputs_.door_open = open; }
    void set_battery(uint16_t permille) { inputs_.battery_permille = permille; }
    void set_ambient(uint16_t permille) { inputs_.ambient_permille = permille; }
    void set_comms_lost(bool lost) { core_.set_comms_lost(lost); }

    /// Step the lighting mode the way a dedicated light switch would.
    void next_light_mode() { core_.request_light_mode(); advance(10U); }

    /* ---- Time ------------------------------------------------------------ */

    /// Advance simulated time, running the control cycle at its real period.
    void advance(uint32_t ms);

    uint32_t now_ms() const { return now_ms_; }

    /* ---- Assertions ------------------------------------------------------ */

    void expect_lamp(services::LampId id, bool lit, const char* what);
    void expect_lamp_blinking(services::LampId id, const char* what);
    void expect_light_state(services::LightState state, const char* what);
    void expect_indicator(services::IndState state, const char* what);
    void expect_door(services::DoorState state, const char* what);
    void expect_power(services::PowerState state, const char* what);
    void expect_dtc(services::Dtc code, bool active, const char* what);
    void expect(bool condition, const char* what);

    /* ---- Scenario bookkeeping -------------------------------------------- */

    void begin(const char* name, const char* requirement);
    ScenarioResult end();

    services::BcmCore& core() { return core_; }

private:
    void record(bool passed, const std::string& description,
                const std::string& detail);
    bool* control_ptr(const char* control);

    services::Config   cfg_;
    services::BcmCore  core_;
    services::Inputs   inputs_;
    uint32_t           now_ms_;
    ScenarioResult     current_;
};

}  // namespace sil
}  // namespace bcm

#endif /* BCM_SIL_RUNNER_H */
