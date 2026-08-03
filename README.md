# Automotive Body Control Module (BCM) Simulator

A production-style **Body Control Module** ECU built the way an automotive
Tier-1/OEM would build it — layered architecture, FreeRTOS, safety-prioritised
state machines, a framed diagnostic protocol, host unit tests, a Software-in-the-Loop
harness, and a C# WPF diagnostic tool — running on a **US $3 STM32F103 "Blue Pill."**

> Not a blink demo. The goal is to demonstrate the design, implementation, test
> and documentation practices used for real vehicle body electronics.

![lang](https://img.shields.io/badge/firmware-C%2B%2B14-orange)
![rtos](https://img.shields.io/badge/RTOS-FreeRTOS%2011.1-green)
![target](https://img.shields.io/badge/MCU-STM32F103C8T6-lightgrey)
![tests](https://img.shields.io/badge/tests-189%20unit%20%2B%2015%20SIL-blue)
![flash](https://img.shields.io/badge/flash-38%25-brightgreen)

**Status: v1.0.0 — complete and running on hardware.**

---

## Features

| Domain | Functions |
|--------|-----------|
| **Exterior lighting** | Parking, DRL, Low/High Beam, auto-headlight with hysteresis |
| **Indicators** | Left, Right, Hazard (hazard overrides) |
| **Safety lamps** | Brake (highest priority), Reverse (reverse-gear only) |
| **Doors** | Lock/Unlock, Welcome, Auto-Lock (never with a door open) |
| **Horn** | Buzzer, lock/unlock chirps, rate limiting |
| **Power** | Sleep/Wake/Run/Shutdown, battery protection, load shedding |
| **Comms** | Framed UART protocol @115200, CRC-8, command dispatcher |
| **Diagnostics** | POST, DTCs, health, event log, unsolicited event frames |
| **Safety** | Central lamp arbiter, watchdog supervisor, comm-timeout safe state |

---

## Architecture

```
Application  →  Service/Manager  →  Driver  →  HAL/BSP  →  STM32 HAL/CMSIS  →  HW
```

Dependencies point downward only, and **no service includes a HAL header**.
That one rule is what lets the control logic run on a PC: 189 unit tests and 15
SIL scenarios execute against the *same source files* the firmware links, not a
reimplementation.

The safety invariants live in exactly one place — `LampArbiter` — which runs
**last, immediately before anything reaches a pin**, so neither a feature nor a
diagnostic host can route around it.

---

## Quick start

```powershell
tools\bcm.ps1 build     # compile the firmware
tools\bcm.ps1 flash     # program the board over SWD
tools\bcm.ps1 verify    # confirm the CPU is executing
tools\bcm.ps1 test      # 189 host unit tests
tools\bcm.ps1 sil       # 15 whole-vehicle scenarios
tools\bcm.ps1 talk      # exercise the protocol over serial
tools\bcm.ps1 desktop   # build and launch the WPF diagnostic tool
tools\bcm.ps1 run       # run under QEMU — no board required
tools\bcm.ps1 docs      # generate the Doxygen API reference
```

`bcm.ps1` locates every toolchain itself; none need to be on `PATH`. Setup is
in the [User Manual](docs/manual/UserManual.md).

---

## Verification

| Level | Coverage | Result |
|---|---|---|
| Unit (Catch2, host) | Each module against hostile inputs | **189 cases / 3630 assertions** |
| Integration (SIL, host) | The assembled BCM over simulated time | **15 scenarios / 79 checks** |
| Interface (xUnit, host) | C# host vs. the ICD, byte-for-byte | **17 tests** |
| System (hardware) | Real board, real serial link | **6/6 protocol checks** |

Three of the four run with no hardware attached and exit non-zero on failure,
so they gate cleanly in CI.

---

## Repository layout

```
firmware/   embedded target  (core, bsp, common, drivers, services, app)
sil/        host simulator (Software-in-the-Loop)
tests/      Catch2 suites (host)
desktop/    C# WPF diagnostic tool (MVVM)
docs/       SRS, architecture, ICD, UML, test report, manual, API
tools/      bcm.ps1
```

---

## Documentation

| Document | ID |
|---|---|
| [Software Requirements Specification](docs/requirements/SRS.md) | BCM-SRS-001 |
| [Architecture Document](docs/architecture/ArchitectureDocument.md) | BCM-ARC-001 |
| [Interface Control Document](docs/protocol/ICD.md) | BCM-ICD-001 |
| [UML Model](docs/uml/UML.md) | BCM-UML-001 |
| [Test Report & Traceability](docs/test/TestReport.md) | BCM-TST-001 |
| [User Manual](docs/manual/UserManual.md) | BCM-MAN-001 |
| [Release Notes](docs/ReleaseNotes.md) | — |

---

## Hardware

STM32F103C8T6 Blue Pill · ST-Link V2 · USB-UART adapter · 10 LEDs (lamps) ·
6 push buttons (switches) · buzzer (horn) · potentiometer (battery voltage) ·
breadboard. The wiring map is `firmware/bsp/board_config.h`, and it is the only
file that knows it.

> **USART1 is remapped to PB6/PB7** — PA9 drives the door-lock lamp on this
> board. Details and the alternatives considered are in the ICD.

---

## Development roadmap

| Phase | Scope | Status |
|-------|-------|--------|
| **0** | Repo, build system, architecture, SRS, buildable skeleton | ✅ |
| **1** | Driver layer (GPIO/Button/LED/UART/Timer/ADC/Watchdog) + unit tests | ✅ |
| **2** | UART protocol: framing, checksum, parser, dispatcher (+ICD) | ✅ |
| **3** | Services + 4 state machines + safety arbitration | ✅ |
| **4** | FreeRTOS tasks, queues, semaphores, event groups | ✅ |
| **5** | SIL harness + automated scenarios + pass/fail report | ✅ |
| **6** | C# WPF diagnostic tool (MVVM, dark theme) | ✅ |
| **7** | UML set, Doxygen, full doc package, release notes | ✅ |

Known limitations are listed honestly in the
[Release Notes](docs/ReleaseNotes.md) — four inputs are not yet wired, and
`config_mgr` has no flash persistence.

---

## License
MIT — see [LICENSE](LICENSE).
