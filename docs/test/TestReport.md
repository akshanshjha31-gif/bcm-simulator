# Test Report & Requirements Traceability
### Automotive Body Control Module (BCM) Simulator

| | |
|---|---|
| **Document ID** | BCM-TST-001 |
| **Version** | 1.0 |
| **Software version** | 1.0.0 |
| **Traces** | `BCM-SRS-001`, `BCM-ARC-001`, `BCM-ICD-001` |
| **Target** | STM32F103C8 "Blue Pill", 72 MHz Cortex-M3 |

---

## 1. Summary

| Level | What it proves | Result |
|---|---|---|
| **Unit** (Catch2, host) | Each module in isolation | **189 cases / 3630 assertions — PASS** |
| **Integration** (SIL, host) | The assembled BCM over simulated time | **15 scenarios / 79 checks — PASS** |
| **Interface** (xUnit, host) | The C# host agrees with the ICD | **17 tests — PASS** |
| **System** (hardware) | The real board over a real serial link | **6/6 protocol checks — PASS** |
| **Hardware bring-up** | Every physical connection | **PASS** — 10 lamps, 6 switches, pot, buzzer |

Nothing is outstanding, waived, or expected-to-fail.

### 1.1 Footprint

```
text 24564   data 124   bss 8456
flash  24.7 kB / 64 kB   = 38 %
RAM     8.5 kB / 20 kB   = 43 %   (incl. 5 task stacks + idle)
```

Measured with `arm-none-eabi-size` on the release image. Satisfies
SRS-PERF-003.

---

## 2. Test strategy

The architecture's one non-negotiable rule — dependencies point downward, and
no service includes a HAL header — is what makes this strategy possible. Three
of the four levels above run **natively on a PC**, against the *same source
files* the firmware links, not a reimplementation.

```
firmware/services/*.cpp  ──┬──> firmware (arm-none-eabi-g++)
                           ├──> unit tests   (mingw g++ + Catch2)
                           └──> SIL harness  (mingw g++)
```

Consequently a SIL scenario failing is a **real defect**, not a model
disagreeing with reality.

### 2.1 What each level is for

- **Unit** — one module, hostile inputs, edge cases. Every single-bit error in
  a CRC, every wraparound in a ring buffer, saturating deltas, boundary
  conditions.
- **SIL** — modules *interacting* over time: does brake priority still hold
  while load shedding is active? Unit tests cannot reach that.
- **Interface** — two independent implementations of BCM-ICD-001 (C++ and C#)
  asserting the *same documented bytes*. This is the evidence that the ICD is
  unambiguous.
- **System** — the assembled product, on real silicon, over a real UART.

---

## 3. Unit test results

Run with `tools\bcm.ps1 test`.

| Suite | Module under test | Cases |
|---|---|---|
| `test_debounce` | `Debouncer` | 10 |
| `test_blink` | `Blinker` | 11 |
| `test_ring_buffer` | `RingBuffer` | 9 |
| `test_filter` | `ExpFilter`, `Hysteresis` | 10 |
| `test_soft_timer` | `SoftTimer` | 9 |
| `test_crc8` | `Crc8` | 8 |
| `test_frame_codec` | `FrameCodec`, `FrameParser` | 15 |
| `test_dispatcher` | `Dispatcher`, `CommMgr` | 16 |
| `test_icd_examples` | BCM-ICD-001 conformance | 8 |
| `test_fsm` | `Fsm` engine | 9 |
| `test_lighting` | `LightingMgr` | 15 |
| `test_door` | `DoorMgr` | 15 |
| `test_arbiter` | `LampArbiter` | 14 |
| `test_power_fault` | `PowerMgr`, `FaultMgr` | 20 |
| `test_logger_diag` | `Logger`, `DiagMgr` | 20 |
| | **Total** | **189** |

```
All tests passed (3630 assertions in 189 test cases)
```

### 3.1 Cases worth calling out

Tests that pin down a property which would otherwise be easy to regress:

| Test | Why it exists |
|---|---|
| *A refusing guard is not bypassed by a later matching row* | If the FSM engine fell through to the next row after a guard refused, a catch-all added later would silently defeat a safety rule |
| *CRC detects byte reordering* / *two-bit error in the same column* | Exactly the failures a XOR checksum cannot see — the reason CRC-8 was chosen |
| *Payload bytes equal to HDR or FTR are carried transparently* | Payloads are unescaped; this breaks a naive parser that scans for the footer |
| *Truncation costs at most one following frame* | A corrupt frame must never desynchronise the link permanently |
| *Overflow drops the OLDEST record* | During a fault the recent events are the ones that explain it |
| *An untested subsystem is NOT a pass* | Treating "no result" as success is how a POST comes to mean nothing |
| *The first battery reading cannot cause a spurious shed* | An unsettled ADC sample would otherwise blank the lamps at power-up |

---

## 4. SIL scenario results

Run with `tools\bcm.ps1 sil`. Simulated time advances at the firmware's real
5 ms control period, so debounce windows, blink phases and auto-lock delays
are exercised at the same granularity the target runs at.

| Scenario | Requirement | Result |
|---|---|---|
| Cold start is dark and asleep | SRS-PWR-001, SRS-PWR-003 | PASS |
| Ignition wakes and shuts down the BCM | SRS-PWR-001 | PASS |
| Light switch steps through every mode | SRS-LIGHT-001 | PASS |
| Holding ignition steps the lights, tapping does not | bring-up affordance | PASS |
| Auto-headlight follows ambient light | SRS-LIGHT-003 | PASS |
| Indicators flash and cancel | SRS-IND-001, SRS-IND-004 | PASS |
| Hazard overrides a running indicator | SRS-IND-003 | PASS |
| Brake lamp overrides everything | SRS-BRK-001, SRS-SAFETY-003 | PASS |
| Reverse lamp only in reverse gear | SRS-REV-001, SRS-SAFETY-005 | PASS |
| Doors never lock while one is open | SRS-DOOR-002, SRS-SAFETY-004 | PASS |
| Auto-lock engages after the configured delay | SRS-DOOR-003 | PASS |
| Losing the diagnostic link drives a defined state | SRS-SAFETY-007 | PASS |
| Load shedding recovers with hysteresis | SRS-PWR-004 | PASS |
| Sleep blanks every lamp but the lock indicator | SRS-PWR-003 | PASS |
| A complete drive cycle | composite | PASS |

```
scenarios: 15  passed: 15  failed: 0
checks:    79  passed: 79  failed: 0
RESULT: PASS
```

---

## 5. System test results (hardware)

Executed against a physical STM32F103C8 with 10 LEDs, 6 buttons, a
potentiometer and a buzzer, flashed over ST-Link V2 and interrogated over a
USB-UART adapter on PB6/PB7 at 115200 8N1.

| Check | Evidence |
|---|---|
| Image programmes and verifies | `** Verified OK **` from OpenOCD |
| CPU executes | `GPIOC->ODR` bit 13 sampled toggling over SWD |
| Scheduler runs | heartbeat driven by the Monitor task |
| `PING` round-trip | payload echoed |
| `GET_VERSION` | v1.0.0 |
| `GET_STATUS` | live lamp + switch bitmaps |
| `GET_BATTERY` | tracks the potentiometer |
| Unknown command | answered `UNKNOWN_CMD`, not ignored |
| Malformed `SET_LAMP` | answered `BAD_LENGTH` |
| WPF tool | connects, holds the port, no exceptions |

```
PASS: 6/6 protocol checks OK.
```

---

## 6. Requirements traceability matrix

| Requirement | Implemented in | Verified by |
|---|---|---|
| SRS-PWR-001 Power modes | `power_mgr` | `test_power_fault`, SIL *ignition-cycle* |
| SRS-PWR-002 Inactivity sleep | `power_mgr` | `test_power_fault` |
| SRS-PWR-003 Sleep blanks lamps | `lamp_arbiter` | `test_arbiter`, SIL *sleep-blanks-lamps* |
| SRS-PWR-004 Load shedding | `power_mgr`, `lamp_arbiter` | `test_arbiter`, SIL *battery-recovery* |
| SRS-LIGHT-001 Lighting FSM | `lighting_mgr` | `test_lighting`, SIL *light-cycle* |
| SRS-LIGHT-003 Auto-headlight | `lighting_mgr`, `filter` | `test_lighting`, SIL *auto-headlight* |
| SRS-LIGHT-004 Parking independent | `lighting_mgr` | `test_lighting` |
| SRS-IND-001 Indicators flash | `blink`, `lighting_mgr` | `test_blink`, `test_lighting` |
| SRS-IND-002 ~1.5 Hz rate | `blink`, `bcm_config` | `test_blink` |
| SRS-IND-003 Hazard overrides | `lighting_mgr` | `test_lighting`, SIL *hazard-override* |
| SRS-IND-004 Starts in ON phase | `blink` | `test_blink` |
| SRS-BRK-001 Brake highest priority | `lamp_arbiter` | `test_arbiter`, SIL *brake-priority* |
| SRS-REV-001 Reverse gear only | `lamp_arbiter` | `test_arbiter`, SIL *reverse-gate* |
| SRS-DOOR-001 Lock / unlock | `door_mgr` | `test_door` |
| **SRS-DOOR-002 No lock, door open** | `door_mgr` guard | `test_door` ×3, SIL *door-open-guard* |
| SRS-DOOR-003 Auto-lock delay | `door_mgr`, `soft_timer` | `test_door`, SIL *auto-lock* |
| SRS-DOOR-004 Welcome lighting | `door_mgr` | `test_door`, SIL *auto-lock* |
| SRS-HORN-001 Horn follows input | `horn_mgr` | SIL, hardware |
| SRS-HORN-002 Lock/unlock chirps | `horn_mgr`, `door_mgr` | `test_door` |
| SRS-SENS-001 Filtered analog | `filter`, `drv_adc` | `test_filter` |
| SRS-SENS-002 Discrete inputs | `debounce`, `drv_button` | `test_debounce` |
| SRS-COM-001 Framed protocol | `frame_codec` | `test_frame_codec`, `test_icd_examples` |
| SRS-COM-002 Corrupt frames rejected | `crc8`, `frame_codec` | `test_crc8`, `test_frame_codec` |
| SRS-COM-003 Command set | `dispatcher`, `commands` | `test_dispatcher`, hardware |
| SRS-DIAG-001 POST | `diag_mgr` | `test_logger_diag` |
| SRS-DIAG-002 DTC store | `fault_mgr` | `test_power_fault` |
| SRS-DIAG-003 Health | `diag_mgr` | `test_logger_diag` |
| SRS-LOG-001 Event log | `logger` | `test_logger_diag` |
| SRS-PERF-001 ≤50 ms switch-to-lamp | `debounce`, 5 ms cycle | `test_debounce` (explicit budget test) |
| SRS-PERF-003 Memory budget | whole image | §1.1, `arm-none-eabi-size` |
| SRS-REL-002 No shared mutable globals | task design | design review — §7.2 |
| SRS-SAFETY-003 Brake priority | `lamp_arbiter` | `test_arbiter` |
| SRS-SAFETY-004 No auto-lock, door open | `door_mgr` guard | `test_door`, SIL |
| SRS-SAFETY-005 Reverse gated | `lamp_arbiter` | `test_arbiter`, SIL |
| SRS-SAFETY-006 Watchdog supervisor | `rtos_app`, `drv_watchdog` | design review + hardware |
| SRS-SAFETY-007 Comm timeout safe state | `comm_mgr`, `lamp_arbiter` | `test_dispatcher`, SIL *comms-loss* |
| SRS-MNT-002 Host-compilable logic | layering | 189 host tests exist at all |

---

## 7. Verification by review

Two requirements are not amenable to automated test on this hardware and were
verified by inspection.

### 7.1 SRS-SAFETY-006 — watchdog supervisor

The IWDG is refreshed only inside `task_watchdog`, and only when
`xEventGroupWaitBits` returns with **every** task's check-in bit set *and*
`DiagMgr::healthy()` is true. The event group is cleared on exit, so each task
must check in again within every window. A hung task therefore stops the
refresh and the MCU resets into a defined state.

Exercising this on hardware would mean deliberately hanging a task; the
mechanism is instead confirmed by inspection plus the observation that the
board runs indefinitely without spurious resets.

### 7.2 SRS-REL-002 — no shared mutable globals

Every state machine is owned by exactly one task (Control), so no manager is
reachable from two contexts and no mutex is required. The only cross-task
datum is the battery reading, a single `volatile uint16_t` — an aligned 16-bit
load/store is atomic on Cortex-M3.

---

## 8. Defects found and fixed during verification

Recorded because how they were found is itself evidence about the process.

| # | Defect | Found by |
|---|---|---|
| 1 | `FrameParser` statistics counters never initialised | Code review while writing tests |
| 2 | ICD worked-example checksums hand-computed and wrong | Computing them properly, then asserting them in `test_icd_examples` |
| 3 | `bcm.ps1 test` reported PASS while a test failed (`Start-Process` does not set `$LASTEXITCODE`) | Noticing a green result with a red test |
| 4 | SysTick routed at the kernel before the scheduler existed → hard fault in clock config | GDB backtrace on target |
| 5 | Part implements **3** NVIC priority bits, not the 4 ST declares | FreeRTOS port assertion, then reading `NVIC IPR0` |
| 6 | `drv_uart::write` emitted **truncated** frames when the TX buffer filled | Raw serial capture after a SIL-clean build failed on hardware |
| 7 | Log text interleaved with binary frames on one wire | Same capture |
| 8 | Test host assumed the next frame was its reply | Consequence of fixing 7 |
| 9 | WPF: `RefreshPorts()` ran before commands were constructed → null deref | Crash log added to the app |
| 10 | WPF: `ProgressBar.Value` binds TwoWay onto a read-only property | Crash log |

Items 4–8 were only reachable by running on real hardware, which is the
argument for keeping a physical target in the loop rather than trusting the
SIL alone.

---

## 9. Known limitations

Stated rather than hidden.

| Limitation | Impact |
|---|---|
| Reverse gear, door-ajar, light-mode switches and the parking lamp are **not wired** | Those inputs are forced inactive on target; exercised in SIL and over the diagnostic link only. Pins are reserved behind `BCM_HW_EXT_IO` |
| No LDR fitted | Auto-headlight reports full daylight on hardware; verified in SIL |
| `config_mgr` has no flash persistence | Tunables are compile-time defaults |
| Watchdog supervision not fault-injected | See §7.1 |
| QEMU timing is not cycle-accurate | 722 ms measured for a 500 ms delay; hardware is exact. QEMU is for logic, not timing |

---

## 10. Revision history

| Version | Change |
|---|---|
| 1.0 | Initial issue covering software version 1.0.0 |
