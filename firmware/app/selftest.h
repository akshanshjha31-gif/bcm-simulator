/**
 * @file    selftest.h
 * @brief   Board bring-up self-test (Phase 1 entry point).
 *
 * Staged deliberately: prove the output wiring first with a dead-simple
 * all-on test, and only then move to the interactive input test. Switching
 * stage is a one-line change in selftest_run().
 */
#ifndef BCM_SELFTEST_H
#define BCM_SELFTEST_H

namespace bcm {
namespace app {

/// Stage 1 - drive every lamp ON and hold it. Heartbeat keeps blinking so a
/// dead board is distinguishable from a wiring fault. Does not return.
void selftest_lamps_on();

/// Stage 2 - mirror each switch onto its lamp, plus the potentiometer.
/// Does not return.
void selftest_mirror();

/// Whichever stage is currently selected. Does not return.
void selftest_run();

}  // namespace app
}  // namespace bcm

#endif /* BCM_SELFTEST_H */
