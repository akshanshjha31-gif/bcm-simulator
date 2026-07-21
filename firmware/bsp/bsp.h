/**
 * @file    bsp.h
 * @brief   Board Support Package - thin C++ facade over board wiring.
 *
 * The BSP is the only layer permitted to touch board_config.h pin macros.
 * In Phase 0 it exposes just enough to prove the toolchain end-to-end; the
 * full GPIO/UART/timer bring-up arrives with the driver layer in Phase 1.
 */
#ifndef BCM_BSP_H
#define BCM_BSP_H

namespace bcm {
namespace bsp {

/// Initialise board-level peripherals (clocks already configured by main).
void init();

/// Toggle the on-board heartbeat LED. Proves the system is alive.
void heartbeat_toggle();

/// Drive the heartbeat LED to an explicit state (true = visibly on).
void heartbeat_set(bool on);

}  // namespace bsp
}  // namespace bcm

#endif /* BCM_BSP_H */
