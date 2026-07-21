# Software Requirements Specification (SRS)
### Automotive Body Control Module (BCM) Simulator

| | |
|---|---|
| **Document ID** | BCM-SRS-001 |
| **Version** | 0.1 (Phase 0 baseline) |
| **Status** | Draft |
| **Target ECU** | STM32F103C8T6 (Cortex-M3, 72 MHz, 64 KB flash / 20 KB RAM) |
| **Standard basis** | IEEE 830-style structure; requirement style informed by ISO 26262 / ASPICE practice |

> This SRS is a *living* document. Phase 0 establishes the full requirement set
> and IDs so later phases implement against a stable contract. Each requirement
> is traceable forward to design (SDD), code, and tests.

---

## 1. Introduction

### 1.1 Purpose
Specify the functional and non-functional requirements of the **Body Control
Module (BCM) Simulator** — an embedded ECU that controls simulated vehicle body
functions (exterior/interior lighting, doors, horn, power modes) and exposes a
diagnostic interface to a Windows desktop tool over UART.

### 1.2 Scope
The BCM runs on a low-cost STM32F103 "Blue Pill" driving LEDs (as lamps), push
buttons (as switches), a buzzer (as the horn) and analog sensors (potentiometer
= battery voltage, LDR = ambient light). It is representative of a real
automotive BCM in **architecture, safety logic, diagnostics and communication**,
without requiring vehicle-grade hardware.

Out of scope: CAN/LIN bus (UART is used as the transport in this project),
functional-safety certification, and physical actuator loads.

### 1.3 Definitions & Abbreviations
| Term | Meaning |
|------|---------|
| BCM | Body Control Module |
| DRL | Daytime Running Lights |
| ECU | Electronic Control Unit |
| FSM | Finite State Machine |
| HAL / BSP | Hardware Abstraction Layer / Board Support Package |
| IGN | Ignition |
| SIL | Software-in-the-Loop |
| DTC | Diagnostic Trouble Code |
| ISR | Interrupt Service Routine |

### 1.4 References
- STM32F103x8/xB Reference Manual (RM0008), Datasheet DS5319
- FreeRTOS Kernel documentation
- Project documents: `BCM-SDD-001` (design), `BCM-ICD-001` (protocol), `BCM-TP-001` (test plan)

---

## 2. Overall Description

### 2.1 Product perspective
The BCM is one node in a two-part system:

```
+-------------------+        UART 115200 8N1        +----------------------+
|  BCM firmware     |  <-------------------------->  |  WPF Diagnostic Tool |
|  (STM32F103)      |    custom framed protocol      |  (C# / MVVM, Windows)|
+-------------------+                                +----------------------+
        |  GPIO / ADC / PWM
        v
  LEDs, buttons, buzzer, potentiometer, LDR
```

### 2.2 User classes
- **Driver (simulated):** operates switches (ignition, lights, indicators, doors).
- **Diagnostic technician:** uses the desktop tool to read status, faults, logs.
- **Developer:** builds, flashes, runs unit tests and SIL.

### 2.3 Operating environment
Bare-metal STM32F103 under FreeRTOS. Desktop tool on Windows 10/11 (.NET 8).

### 2.4 Design & implementation constraints
- Firmware in **C++14** (application/services) + C (CMSIS/HAL/startup).
- Layered architecture; no global mutable state across module boundaries.
- Must fit in 64 KB flash / 20 KB RAM with headroom.
- MISRA-inspired coding rules where practical (no dynamic allocation on hot paths, const-correctness, strong typing).

### 2.5 Assumptions & dependencies
- 8 MHz HSE crystal present (Blue Pill standard) → 72 MHz SYSCLK.
- USB-UART adapter provides the diagnostic link.

---

## 3. Functional Requirements

Requirement key: **`SRS-<AREA>-<NNN>`**. Priority: M (mandatory), S (should), C (could).

### 3.1 Ignition & Power (PWR)
| ID | Pri | Requirement |
|----|-----|-------------|
| SRS-PWR-001 | M | The BCM shall support power states **Sleep → Wake → Run → Shutdown** and transition between them per the Power FSM. |
| SRS-PWR-002 | M | On ignition ON the BCM shall enter **Run** within 100 ms. |
| SRS-PWR-003 | M | In **Sleep**, all lamp outputs shall be OFF except those explicitly allowed (e.g. welcome/leaving-home sequences). |
| SRS-PWR-004 | S | The BCM shall monitor battery voltage and enter a **battery-protection** mode below a low-voltage threshold, shedding non-critical loads. |

### 3.2 Exterior Lighting (LIGHT)
| ID | Pri | Requirement |
|----|-----|-------------|
| SRS-LIGHT-001 | M | The BCM shall control **Parking, DRL, Low Beam, High Beam** via the Lighting FSM (OFF → Parking → DRL → Low Beam → High Beam). |
| SRS-LIGHT-002 | M | **High Beam shall be permitted only when Low Beam is active.** |
| SRS-LIGHT-003 | S | With auto-headlight enabled, the BCM shall switch Low Beam ON/OFF based on filtered ambient-light (LDR) crossing configured thresholds with hysteresis. |
| SRS-LIGHT-004 | M | Parking lights shall be independently switchable and remain permitted in low power states. |

### 3.3 Indicators & Hazards (IND)
| ID | Pri | Requirement |
|----|-----|-------------|
| SRS-IND-001 | M | The BCM shall provide **Left, Right, Hazard** indicator functions via the Indicator FSM (Idle → Left/Right → Hazard). |
| SRS-IND-002 | M | Indicators shall flash at **1.5 Hz ± 10 %** (configurable). |
| SRS-IND-003 | M | **Hazard shall override** left/right indicator requests and flash both sides. |
| SRS-IND-004 | S | On indicator activation the corresponding lamp shall start in the ON phase (no dark first blink). |

### 3.4 Brake / Reverse (SAFE-lamps) (BRK/REV)
| ID | Pri | Requirement |
|----|-----|-------------|
| SRS-BRK-001 | M | Brake lamp shall follow the brake switch with **highest lamp priority** (always overrides conflicting lamp logic on shared outputs). |
| SRS-REV-001 | M | Reverse lamp shall be ON **only when reverse gear is selected**. |

### 3.5 Doors (DOOR)
| ID | Pri | Requirement |
|----|-----|-------------|
| SRS-DOOR-001 | M | The BCM shall control **Lock / Unlock** and run **Welcome** and **Auto-Lock** sequences via the Door FSM. |
| SRS-DOOR-002 | M | **The BCM shall not auto-lock while any door is open.** |
| SRS-DOOR-003 | S | Auto-lock shall engage after a configurable delay once speed exceeds a threshold (simulated). |
| SRS-DOOR-004 | C | **Welcome lighting** shall activate on unlock; **Leaving-home lighting** shall activate on lock at night. |

### 3.6 Horn (HORN)
| ID | Pri | Requirement |
|----|-----|-------------|
| SRS-HORN-001 | M | The BCM shall sound the buzzer while the horn input is asserted. |
| SRS-HORN-002 | S | Lock/unlock confirmation chirps shall be supported and rate-limited. |

### 3.7 Sensors (SENS)
| ID | Pri | Requirement |
|----|-----|-------------|
| SRS-SENS-001 | M | The BCM shall acquire **ambient light** and **battery voltage** via ADC and apply digital filtering (moving average / EWMA). |
| SRS-SENS-002 | M | The BCM shall read discrete inputs: ignition, brake switch, door switch, reverse gear, seat belt. |
| SRS-SENS-003 | S | Discrete inputs shall be **debounced** (configurable, default 20 ms). |

### 3.8 Communication (COM)
| ID | Pri | Requirement |
|----|-----|-------------|
| SRS-COM-001 | M | The BCM shall implement a **framed UART protocol** (Header, Command, Length, Payload, Checksum, Footer) at 115200 8N1. |
| SRS-COM-002 | M | The BCM shall validate the checksum and reject malformed frames without corrupting state. |
| SRS-COM-003 | M | Supported commands shall include: lamp on/off, read door/ignition/battery status, lock/unlock, reset, **heartbeat**, version request. |
| SRS-COM-004 | M | The BCM shall respond to a **heartbeat** within 100 ms; loss of heartbeat for > 2 s shall raise a communication-timeout fault. |

### 3.9 Diagnostics & Faults (DIAG)
| ID | Pri | Requirement |
|----|-----|-------------|
| SRS-DIAG-001 | M | The BCM shall run a **power-on self-test (POST)** and report pass/fail per module. |
| SRS-DIAG-002 | M | The BCM shall maintain **DTCs** with set/clear semantics and report them on request. |
| SRS-DIAG-003 | M | The BCM shall expose **module health** and a periodic heartbeat to the desktop tool. |
| SRS-DIAG-004 | S | The BCM shall support **fault recovery** where safe, and log every set/clear transition. |

### 3.10 Event Logging (LOG)
| ID | Pri | Requirement |
|----|-----|-------------|
| SRS-LOG-001 | M | The BCM shall log timestamped events (e.g. Ignition ON, Door Locked, Brake Applied, Low Beam ON, Comm Lost, Watchdog Reset). |
| SRS-LOG-002 | S | Logs shall be retrievable by the desktop tool and displayed in a live view. |

### 3.11 Configuration (CFG)
| ID | Pri | Requirement |
|----|-----|-------------|
| SRS-CFG-001 | S | Tunable parameters (blink rate, thresholds, delays, debounce) shall be centralised in a configuration manager. |
| SRS-CFG-002 | C | Configuration shall be persistable to on-chip flash and restored on boot with integrity check. |

---

## 4. Safety Requirements (SAFETY)
These are cross-cutting invariants enforced regardless of command source.

| ID | Pri | Requirement |
|----|-----|-------------|
| SRS-SAFETY-001 | M | Hazard overrides indicators (see SRS-IND-003). |
| SRS-SAFETY-002 | M | High Beam only when Low Beam active (see SRS-LIGHT-002). |
| SRS-SAFETY-003 | M | Brake lamp has highest priority (see SRS-BRK-001). |
| SRS-SAFETY-004 | M | No auto-lock while a door is open (see SRS-DOOR-002). |
| SRS-SAFETY-005 | M | Reverse lamp only in reverse gear (see SRS-REV-001). |
| SRS-SAFETY-006 | M | An **independent watchdog (IWDG)** shall reset the ECU if the supervisor task fails to service it within the timeout. |
| SRS-SAFETY-007 | M | Communication timeout shall drive lamps/doors to a defined safe state, not an undefined one. |

---

## 5. Non-Functional Requirements

### 5.1 Performance (PERF)
| ID | Requirement |
|----|-------------|
| SRS-PERF-001 | Switch-to-lamp reaction (debounced input to output) ≤ **50 ms**. |
| SRS-PERF-002 | Indicator flash jitter ≤ **±5 %** of period. |
| SRS-PERF-003 | Firmware image shall occupy ≤ **80 %** of flash and ≤ **80 %** of RAM at release. |

### 5.2 Reliability & Safety-integrity
| ID | Requirement |
|----|-------------|
| SRS-REL-001 | No dynamic memory allocation after initialisation on real-time paths. |
| SRS-REL-002 | All inter-task communication via RTOS primitives (queues/semaphores/event groups), not shared globals. |

### 5.3 Maintainability & Portability
| ID | Requirement |
|----|-------------|
| SRS-MNT-001 | Every module shall have header, source, docs and unit tests. |
| SRS-MNT-002 | Business logic shall be **host-compilable** (no HAL dependency) to enable SIL and unit testing off-target. |
| SRS-MNT-003 | Board wiring shall be confined to `board_config.h`. |

### 5.4 Testability
| ID | Requirement |
|----|-------------|
| SRS-TST-001 | Logic modules shall reach high unit-test coverage (target ≥ 80 % lines for logic/parsers/FSMs). |
| SRS-TST-002 | A SIL harness shall execute automated scenarios and produce a pass/fail report. |

---

## 6. Requirement Traceability (skeleton)
Populated progressively; each row links a requirement to its design element, source module and test case.

| Requirement | Design (SDD) | Module | Test |
|-------------|--------------|--------|------|
| SRS-COM-001 | Protocol framing | `services/comm` | `test_frame_parser` |
| SRS-LIGHT-002 | Lighting FSM guard | `services/lighting` | `test_lighting_fsm` |
| SRS-DOOR-002 | Door FSM guard | `services/door` | `test_door_fsm` |
| SRS-SAFETY-006 | Watchdog supervisor | `services/watchdog` | on-target + SIL |
| … | … | … | … |

---

## 7. Phase Roadmap (delivery of this SRS)
| Phase | Requirements primarily realised |
|-------|---------------------------------|
| 0 | Project baseline, SRS/architecture, buildable target skeleton |
| 1 | SRS-SENS-*, driver-level parts of LIGHT/IND/DOOR/HORN, SRS-MNT-* |
| 2 | SRS-COM-* |
| 3 | SRS-LIGHT-*, SRS-IND-*, SRS-DOOR-*, SRS-PWR-*, SRS-SAFETY-* |
| 4 | SRS-REL-002, RTOS realisation of all tasks |
| 5 | SRS-TST-002 (SIL) |
| 6 | SRS-LOG-002, desktop diagnostics |
| 7 | Documentation completion, traceability closure |
