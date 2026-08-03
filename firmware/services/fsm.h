/**
 * @file    fsm.h
 * @brief   Reusable table-driven state-machine engine - pure logic, no HAL.
 *
 * All four BCM state machines (Lighting, Indicator, Door, Power) share this
 * engine and differ only in their transition table. That keeps behaviour
 * declarative and inspectable: you can read what a machine does without
 * reading any code (ADR-003).
 *
 * A transition may carry a guard and an action. The guard runs first; if it
 * refuses, the transition does not fire and no action runs. This is how
 * safety invariants like "never auto-lock with a door open" are expressed as
 * data rather than scattered `if` statements (SRS-SAFETY-004).
 */
#ifndef BCM_FSM_H
#define BCM_FSM_H

#include <stdint.h>

namespace bcm {
namespace services {

/**
 * @brief One row of a transition table.
 *
 * @tparam StateT state enum
 * @tparam EventT event enum
 * @tparam CtxT   per-machine context handed to guards and actions
 */
template <typename StateT, typename EventT, typename CtxT>
struct FsmTransition {
    StateT from;
    bool   from_any;                    ///< ignore `from` - match any state
    EventT event;
    StateT to;
    bool (*guard)(const CtxT&);         ///< 0 = always allowed
    void (*action)(CtxT&);              ///< 0 = nothing to do
};

template <typename StateT, typename EventT, typename CtxT>
class Fsm {
public:
    typedef FsmTransition<StateT, EventT, CtxT> Transition;

    Fsm(const Transition* table, uint8_t count, StateT initial)
        : table_(table), count_(count), state_(initial),
          transitions_(0U), blocked_(0U)
    {
    }

    /**
     * @brief Offer an event to the machine.
     * @return true if a transition fired.
     *
     * The first matching row wins, so more specific rows must precede
     * `from_any` catch-alls in the table.
     */
    bool dispatch(EventT event, CtxT& context)
    {
        if (table_ == 0) { return false; }

        for (uint8_t i = 0U; i < count_; ++i) {
            const Transition& t = table_[i];

            if (t.event != event)                  { continue; }
            if (!t.from_any && !(t.from == state_)) { continue; }

            if (t.guard != 0 && !t.guard(context)) {
                /* A guard that refuses is a deliberate outcome, not a
                 * no-match - stop looking so a later row cannot bypass it. */
                ++blocked_;
                return false;
            }

            state_ = t.to;
            ++transitions_;
            if (t.action != 0) { t.action(context); }
            return true;
        }
        return false;
    }

    StateT state() const { return state_; }

    /// Force a state without running guards or actions. Reset paths only.
    void set_state(StateT s) { state_ = s; }

    uint32_t transition_count() const { return transitions_; }

    /// How many times a guard refused an otherwise-matching event.
    uint32_t blocked_count() const { return blocked_; }

private:
    const Transition* table_;
    uint8_t           count_;
    StateT            state_;
    uint32_t          transitions_;
    uint32_t          blocked_;
};

}  // namespace services
}  // namespace bcm

#endif /* BCM_FSM_H */
