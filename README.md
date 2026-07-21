# Automotive Body Control Module (BCM) Simulator

A production-style **Body Control Module** ECU built the way an automotive
Tier-1/OEM would build it — layered architecture, FreeRTOS, safety-prioritised
state machines, a framed diagnostic protocol, host unit tests, a Software-in-the-Loop
harness, and a C# WPF diagnostic tool — running on a **US $3 STM32F103 "Blue Pill."**

> Not a blink demo. The goal is to demonstrate the design, implementation, test
> and documentation practices used for real vehicle body electronics.

[![firmware-ci](https://img.shields.io/badge/CI-firmware--build-blue)](.github/workflows/ci.yml)
![lang](https://img.shields.io/badge/firmware-C%2B%2B14-orange)
![rtos](https://img.shields.io/badge/RTOS-FreeRTOS-green)
![target](https://img.shields.io/badge/MCU-STM32F103C8T6-lightgrey)

---

## Features (target scope)

| Domain | Functions |
|--------|-----------|
| **Exterior lighting** | Parking, DRL, Low/High Beam, auto-headlight (LDR) |
| **Indicators** | Left, Right, Hazard (hazard overrides) |
| **Safety lamps** | Brake (highest priority), Reverse (reverse-gear only) |
| **Doors** | Lock/Unlock, Welcome, Auto-Lock (never with a door open) |
| **Horn** | Buzzer + lock/unlock chirps |
| **Power** | Sleep/Wake/Run/Shutdown, battery protection, load shedding |
| **Comms** | Framed UART protocol @115200, command dispatcher |
| **Diagnostics** | POST, DTCs, health, heartbeat, event log |
| **Safety** | Watchdog supervisor, comm-timeout safe state, priority arbitration |

---

## Architecture

```
Application  →  Service/Manager  →  Driver  →  HAL/BSP  →  STM32 HAL/CMSIS  →  HW
```

Dependencies point downward only; business logic is HAL-free so it runs in unit
tests and the SIL on a PC. See **[docs/architecture/ArchitectureDocument.md](docs/architecture/ArchitectureDocument.md)**
and **[docs/requirements/SRS.md](docs/requirements/SRS.md)**.

---

## Repository layout

```
firmware/   embedded target  (core, bsp, common, drivers, services, app)
sil/        host simulator (Software-in-the-Loop)
tests/      GoogleTest suites (host)
desktop/    C# WPF diagnostic tool (MVVM)
docs/       SRS, architecture, protocol (ICD), UML, test, manuals
tools/      helper scripts
```

---

## Hardware (bill of materials)

STM32F103C8T6 Blue Pill · USB-UART adapter · LEDs (lamps) · push buttons
(switches) · buzzer (horn) · potentiometer (battery voltage) · LDR (ambient) ·
breadboard. Wiring map lives in `firmware/bsp/board_config.h`.

---

## Build & flash (firmware)

Toolchain: `arm-none-eabi-gcc` (C++14) + GNU Make. Vendor HAL/CMSIS are fetched,
not committed.

```bash
cd firmware
make deps      # one-time: clone STM32CubeF1 (HAL + CMSIS device submodules)
make           # builds build/bcm_firmware.{elf,hex,bin} and prints size
make flash     # st-flash write to 0x08000000 (ST-Link)
make monitor   # serial terminal hint (USART1 PA9/PA10, 115200 8N1)
```

**Phase 0 status:** builds clean; blinks the on-board PC13 heartbeat at 1 Hz,
proving the full toolchain → 72 MHz clock → HAL → GPIO path.
Image size: `text 3336 · data 20 · bss 1572` bytes.

---

## Development roadmap

| Phase | Scope | Status |
|-------|-------|--------|
| **0** | Repo, build system, architecture, SRS, buildable skeleton | ✅ done |
| 1 | Driver layer (GPIO/Button/LED/UART/Timer/ADC/Watchdog) + unit tests | ⬜ |
| 2 | UART protocol: framing, checksum, parser, dispatcher (+ICD) | ⬜ |
| 3 | Services + 4 state machines + safety arbitration | ⬜ |
| 4 | FreeRTOS tasks, queues, semaphores, event groups, timers | ⬜ |
| 5 | SIL harness + automated scenarios + pass/fail report | ⬜ |
| 6 | C# WPF diagnostic tool (MVVM, dark theme) | ⬜ |
| 7 | UML set, Doxygen, full doc package, release notes | ⬜ |

---

## License
MIT — see [LICENSE](LICENSE).
