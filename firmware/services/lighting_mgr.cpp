/**
 * @file    lighting_mgr.cpp
 * @brief   Lighting and Indicator state machines.
 */
#include "lighting_mgr.h"

namespace bcm {
namespace services {
namespace {

/* ---- Lighting transition table ---------------------------------------- */

void mark_manual(LightingContext& c) { c.manual_override = true; }

void clear_manual(LightingContext& c)
{
    c.manual_override       = false;
    c.auto_headlight_active = false;
}

void mark_auto(LightingContext& c) { c.auto_headlight_active = true; }

void clear_auto(LightingContext& c) { c.auto_headlight_active = false; }

/* Auto-headlight must not fight a driver who has chosen a mode by hand. */
bool auto_allowed(const LightingContext& c) { return !c.manual_override; }

/* Only undo what auto turned on. */
bool auto_owns_lamps(const LightingContext& c) { return c.auto_headlight_active; }

typedef FsmTransition<LightState, LightEvent, LightingContext> LightRow;

const LightRow kLightTable[] = {
    /* from                 any    event                    to                      guard            action */
    { LightState::Off,      false, LightEvent::ModeNext,    LightState::Parking,   0,               mark_manual  },
    { LightState::Parking,  false, LightEvent::ModeNext,    LightState::Drl,       0,               mark_manual  },
    { LightState::Drl,      false, LightEvent::ModeNext,    LightState::LowBeam,   0,               mark_manual  },
    { LightState::LowBeam,  false, LightEvent::ModeNext,    LightState::HighBeam,  0,               mark_manual  },
    /* High beam wraps back to Off, completing the cycle. */
    { LightState::HighBeam, false, LightEvent::ModeNext,    LightState::Off,       0,               clear_manual },

    /* Auto-headlight: only from a dark-but-not-driving-lamps state, and only
     * while the driver has not taken manual control. */
    { LightState::Off,      false, LightEvent::AutoOn,      LightState::LowBeam,   auto_allowed,    mark_auto    },
    { LightState::Parking,  false, LightEvent::AutoOn,      LightState::LowBeam,   auto_allowed,    mark_auto    },
    { LightState::Drl,      false, LightEvent::AutoOn,      LightState::LowBeam,   auto_allowed,    mark_auto    },
    { LightState::LowBeam,  false, LightEvent::AutoOff,     LightState::Off,       auto_owns_lamps, clear_auto   },

    /* Anything can be forced off. */
    { LightState::Off,      true,  LightEvent::AllOff,      LightState::Off,       0,               clear_manual },
};

/* ---- Indicator transition table ---------------------------------------- */

void latch_hazard(IndicatorContext& c)   { c.hazard_latched = true; }
void unlatch_hazard(IndicatorContext& c) { c.hazard_latched = false; }

typedef FsmTransition<IndState, IndEvent, IndicatorContext> IndRow;

const IndRow kIndTable[] = {
    /* Hazard first: it must win from ANY state, including while an
     * indicator is running (SRS-IND-003). */
    { IndState::Idle,   true,  IndEvent::HazardPressed, IndState::Hazard, 0, latch_hazard   },

    /* Directional requests are ignored while hazard is latched - the table
     * simply has no Hazard->Left/Right rows, so the FSM refuses them. */
    { IndState::Idle,   false, IndEvent::LeftPressed,   IndState::Left,   0, 0 },
    { IndState::Right,  false, IndEvent::LeftPressed,   IndState::Left,   0, 0 },
    { IndState::Left,   false, IndEvent::LeftPressed,   IndState::Idle,   0, 0 },

    { IndState::Idle,   false, IndEvent::RightPressed,  IndState::Right,  0, 0 },
    { IndState::Left,   false, IndEvent::RightPressed,  IndState::Right,  0, 0 },
    { IndState::Right,  false, IndEvent::RightPressed,  IndState::Idle,   0, 0 },

    { IndState::Idle,   true,  IndEvent::Cancel,        IndState::Idle,   0, unlatch_hazard },
};

const uint8_t kLightRows = sizeof(kLightTable) / sizeof(kLightTable[0]);
const uint8_t kIndRows   = sizeof(kIndTable) / sizeof(kIndTable[0]);

}  // namespace

LightingMgr::LightingMgr(const Config& config)
    : cfg_(config),
      light_(kLightTable, kLightRows, LightState::Off),
      indicator_(kIndTable, kIndRows, IndState::Idle),
      ctx_(),
      ind_ctx_(),
      dark_(config.ambient_dark_permille, config.ambient_light_permille),
      dark_known_(false)
{
}

void LightingMgr::next_mode()  { light_.dispatch(LightEvent::ModeNext, ctx_); }
void LightingMgr::all_off()
{
    light_.dispatch(LightEvent::AllOff, ctx_);
    light_.set_state(LightState::Off);
}

void LightingMgr::update_ambient(uint16_t permille)
{
    /* Hysteresis is inverted here: "dark" means BELOW the threshold, so feed
     * the complement and let the Schmitt trigger do the rest. */
    const uint16_t inverted = (permille > 1000U) ? 0U : static_cast<uint16_t>(1000U - permille);
    const bool     is_dark  = dark_.update(inverted);

    if (!dark_known_) {
        dark_known_ = true;
        if (!is_dark) { return; }
    }

    if (is_dark) { light_.dispatch(LightEvent::AutoOn, ctx_); }
    else         { light_.dispatch(LightEvent::AutoOff, ctx_); }
}

void LightingMgr::indicator_left()   { indicator_.dispatch(IndEvent::LeftPressed, ind_ctx_); }
void LightingMgr::indicator_right()  { indicator_.dispatch(IndEvent::RightPressed, ind_ctx_); }
void LightingMgr::indicator_cancel() { indicator_.dispatch(IndEvent::Cancel, ind_ctx_); }

void LightingMgr::indicator_hazard()
{
    /* Hazard is a toggle: pressing it while latched returns to Idle. */
    if (ind_ctx_.hazard_latched) {
        indicator_.dispatch(IndEvent::Cancel, ind_ctx_);
    }
    else {
        indicator_.dispatch(IndEvent::HazardPressed, ind_ctx_);
    }
}

void LightingMgr::apply(LampState& out) const
{
    switch (light_.state()) {
    case LightState::Parking:
        out.set(LampId::Drl, LampDrive::On);
        break;
    case LightState::Drl:
        out.set(LampId::Drl, LampDrive::On);
        break;
    case LightState::LowBeam:
        out.set(LampId::Drl, LampDrive::On);
        out.set(LampId::LowBeam, LampDrive::On);
        break;
    case LightState::HighBeam:
        /* High beam never runs alone - low beam stays lit beneath it. */
        out.set(LampId::Drl, LampDrive::On);
        out.set(LampId::LowBeam, LampDrive::On);
        out.set(LampId::HighBeam, LampDrive::On);
        break;
    case LightState::Off:
    default:
        break;
    }

    switch (indicator_.state()) {
    case IndState::Left:
        out.set(LampId::IndLeft, LampDrive::Blink);
        break;
    case IndState::Right:
        out.set(LampId::IndRight, LampDrive::Blink);
        break;
    case IndState::Hazard:
        out.set(LampId::IndLeft, LampDrive::Blink);
        out.set(LampId::IndRight, LampDrive::Blink);
        out.set(LampId::Hazard, LampDrive::Blink);
        break;
    case IndState::Idle:
    default:
        break;
    }
}

void LightingMgr::reset()
{
    light_.set_state(LightState::Off);
    indicator_.set_state(IndState::Idle);
    ctx_        = LightingContext();
    ind_ctx_    = IndicatorContext();
    dark_known_ = false;
}

}  // namespace services
}  // namespace bcm
