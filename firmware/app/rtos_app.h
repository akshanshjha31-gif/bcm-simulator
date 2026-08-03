/**
 * @file    rtos_app.h
 * @brief   FreeRTOS task set and start-up (Phase 4).
 *
 * Task table
 * ----------
 * | Task     | Prio | Period  | Responsibility                              |
 * |----------|------|---------|---------------------------------------------|
 * | Watchdog | 5    | 200 ms  | Refresh IWDG iff every task checked in      |
 * | Monitor  | 4    | 100 ms  | Health aggregation, POST, heartbeat LED     |
 * | Control  | 3    | 5 ms    | Inputs, FSMs, arbitration, outputs, comms   |
 * | Sensor   | 2    | 20 ms   | ADC sampling and filtering                  |
 * | Logger   | 1    | queue   | Drain the log queue to the diagnostic UART  |
 *
 * Deviation from the architecture document, stated plainly
 * -------------------------------------------------------
 * The document lists nine tasks, splitting Lighting, Door and Power apart.
 * They are merged into Control here, deliberately: keeping every state
 * machine under a single owner means no manager is ever touched from two
 * contexts, so the design needs no mutexes around the FSMs at all. Splitting
 * them would buy nothing on a single-core M3 and would introduce exactly the
 * locking that SRS-REL-002 ("no shared mutable globals") is meant to avoid.
 *
 * Everything is statically allocated - see FreeRTOSConfig.h.
 */
#ifndef BCM_RTOS_APP_H
#define BCM_RTOS_APP_H

namespace bcm {
namespace app {

/// Create every task and start the scheduler. Does not return.
void rtos_start();

}  // namespace app
}  // namespace bcm

#endif /* BCM_RTOS_APP_H */
