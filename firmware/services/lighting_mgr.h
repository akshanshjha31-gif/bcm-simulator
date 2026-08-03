/**
 * @file    lighting_mgr.h
 * @brief   Lighting and Indicator state machines - pure logic, no HAL.
 *
 * Lighting:  Off -> Parking -> Drl -> LowBeam -> HighBeam  (cycles)
 *            plus auto-headlight from the ambient sensor.
 * Indicator: Idle -> Left / Right -> Hazard, hazard overriding both.
 *
 * Neither machine touches a pin. Each publishes what it wants lit; the lamp
 * arbiter resolves that against the safety rules.
 *
 * Traces SRS-LIGHT-001..004, SRS-IND-001..004.
 */
#ifndef BCM_LIGHTING_MGR_H
#define BCM_LIGHTING_MGR_H

#include "bcm_config.h"
#include "bcm_types.h"
#include "filter.h"
#include "fsm.h"

namespace bcm {
namespace services {

enum class LightState : uint8_t {
    Off = 0,
    Parking,
    Drl,
    LowBeam,
    HighBeam
};

enum class LightEvent : uint8_t {
    ModeNext,       ///< light switch stepped one position
    AllOff,         ///< forced off (ignition off, load shed)
    AutoOn,         ///< ambient sensor says it is dark
    AutoOff         ///< ambient sensor says it is light again
};

enum class IndState : uint8_t {
    Idle = 0,
    Left,
    Right,
    Hazard
};

enum class IndEvent : uint8_t {
    LeftPressed,
    RightPressed,
    HazardPressed,
    Cancel
};

/// Context handed to guards/actions. Kept minimal on purpose.
struct LightingContext {
    bool auto_headlight_active;
    bool manual_override;      ///< user chose a mode, so auto must not undo it

    LightingContext() : auto_headlight_active(false), manual_override(false) {}
};

struct IndicatorContext {
    bool hazard_latched;

    IndicatorContext() : hazard_latched(false) {}
};

class LightingMgr {
public:
    explicit LightingMgr(const Config& config);

    /// Step the light switch one position (Off -> Parking -> ... -> Off).
    void next_mode();

    /// Force every lamp off - used by load shedding and ignition-off.
    void all_off();

    /// Feed the filtered ambient reading; drives auto-headlight with
    /// hysteresis so dusk does not make the lamps chatter (SRS-LIGHT-003).
    void update_ambient(uint16_t permille);

    /// Indicator inputs. Hazard overrides an active indicator (SRS-IND-003).
    void indicator_left();
    void indicator_right();
    void indicator_hazard();
    void indicator_cancel();

    /// Publish the desired lamp states into @p out.
    void apply(LampState& out) const;

    LightState light_state() const { return light_.state(); }
    IndState   indicator_state() const { return indicator_.state(); }
    bool       auto_headlight() const { return ctx_.auto_headlight_active; }

    void reset();

private:
    const Config&                                          cfg_;
    Fsm<LightState, LightEvent, LightingContext>           light_;
    Fsm<IndState, IndEvent, IndicatorContext>              indicator_;
    LightingContext                                        ctx_;
    IndicatorContext                                       ind_ctx_;
    drivers::Hysteresis                                    dark_;
    bool                                                   dark_known_;
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_LIGHTING_MGR_H */
