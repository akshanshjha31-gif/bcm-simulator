# Architecture Document
### Automotive Body Control Module (BCM) Simulator

| | |
|---|---|
| **Document ID** | BCM-ARC-001 |
| **Version** | 0.1 (Phase 0 baseline) |
| **Traces** | `BCM-SRS-001` |

---

## 1. Architectural goals & drivers
The architecture is shaped by the same forces that drive a production automotive BCM:

1. **Separation of concerns / portability** — hardware-independent logic must be
   testable off-target (unit tests + SIL). *(SRS-MNT-002, SRS-TST-*)*
2. **Determinism & real-time behaviour** — bounded latency, no heap on hot paths,
   RTOS-mediated communication. *(SRS-PERF-*, SRS-REL-*)*
3. **Safety invariants that survive any input** — priority/guard logic centralised,
   not scattered per feature. *(SRS-SAFETY-*)*
4. **Diagnosability** — every module reports health; events are logged and
   externally observable. *(SRS-DIAG-*, SRS-LOG-*)*
5. **Maintainability** — strict layering, one responsibility per module, SOLID
   where it pays off.

---

## 2. Layered architecture

```
        +--------------------------------------------------------------+
        |  Application Layer                                            |
        |  system orchestration, POST, mode coordination, task bodies  |
        +--------------------------------------------------------------+
                                   |  (depends on)
        +--------------------------------------------------------------+
        |  Service / Manager Layer                                     |
        |  Lighting  Indicator  Door  Horn  Power  Fault  Comm         |
        |  Sensor  Config  Diagnostics  Logger  StateMachine engine    |
        +--------------------------------------------------------------+
                                   |
        +--------------------------------------------------------------+
        |  Driver Layer                                                |
        |  GPIO  Button  LED  UART  Timer  ADC  Watchdog  Flash        |
        +--------------------------------------------------------------+
                                   |
        +--------------------------------------------------------------+
        |  HAL / BSP Layer   (board_config.h is the only wiring map)   |
        +--------------------------------------------------------------+
                                   |
        +--------------------------------------------------------------+
        |  STM32 CMSIS + ST HAL  ->  STM32F103 hardware                |
        +--------------------------------------------------------------+
```

**Dependency rule:** dependencies point **downward only**. A layer never calls
up; upper layers are injected with interfaces so lower layers can be mocked. This
is what makes the Service layer host-compilable for SIL/unit tests: on the host,
the Driver layer is replaced by fakes.

### 2.1 Why layered (vs. a flat CubeMX project)
A flat "everything calls HAL" project cannot be unit-tested off-target and tends
to leak board details everywhere. Layering costs a little indirection but buys
testability, portability to another MCU (swap HAL/BSP only), and clear ownership —
exactly the trade real OEM/Tier-1 code makes.

---

## 3. Module catalogue & responsibilities

| Module | Layer | Single responsibility |
|--------|-------|-----------------------|
| `bsp` / `board_config.h` | HAL/BSP | Board wiring, clock bring-up, pin map |
| `drv_gpio` | Driver | Typed digital I/O over HAL GPIO |
| `drv_button` | Driver | Debounced edge/level detection |
| `drv_led` | Driver | Lamp abstraction (on/off/blink/pwm) |
| `drv_uart` | Driver | IT/DMA UART TX/RX with ring buffers |
| `drv_timer` | Driver | Periodic ticks / software-timer base |
| `drv_adc` | Driver | Sampling for ambient light & battery |
| `drv_watchdog` | Driver | IWDG start/refresh |
| `logger` | Service | Timestamped ring-buffer event log |
| `config_mgr` | Service | Central tunables (+ flash persistence) |
| `sensor_mgr` | Service | Filtered sensor values, debounced switches |
| `comm_mgr` | Service | Frame codec, parser, command dispatch |
| `lighting_mgr` | Service | Lighting + Indicator FSMs, lamp arbitration |
| `door_mgr` | Service | Door FSM, welcome/auto-lock, open-door guard |
| `horn_mgr` | Service | Buzzer control, chirps, rate limiting |
| `power_mgr` | Service | Power FSM, load shedding, battery protection |
| `fault_mgr` | Service | DTC store, set/clear, recovery policy |
| `diag_mgr` | Service | POST, health, heartbeat |
| `fsm` | Service | Reusable table-driven state-machine engine |
| `system_mgr` / `app` | Application | Wire-up, orchestration, task bodies |

Each realises **SRS** items and gets header + source + Doxygen + unit tests.

### 3.1 SOLID application (concrete)
- **S**: `comm_mgr` framing is separate from command *semantics* (dispatch table).
- **O**: new commands/lamps added via tables, not by editing switch-ladders.
- **L**: driver fakes for SIL satisfy the same interface as on-target drivers.
- **I**: managers depend on narrow interfaces (`ILamp`, `ISwitch`) not whole drivers.
- **D**: services depend on abstractions; concrete HAL wiring injected at composition root (`app`).

---

## 4. Concurrency model (FreeRTOS, Phase 4)

| Task | Prio | Period / trigger | Responsibility |
|------|------|------------------|----------------|
| Button/Input | High | 5 ms tick | Debounce, post input events |
| Lighting | Med-High | event + 10 ms | Drive Lighting/Indicator FSMs |
| Door | Med | event | Door FSM |
| Sensor | Med | 20 ms | ADC sample + filter |
| Communication | Med | UART RX event | Parse frames, dispatch, respond |
| Logger | Low | queue | Drain log queue to sink |
| System Monitor | High | 100 ms | Health aggregation, heartbeat |
| Watchdog | Highest | 200 ms | Refresh IWDG iff all tasks alive |
| Power Mgmt | Med | event + 100 ms | Power FSM, load shedding |

**Inter-task communication:** queues (input events, log records, comm frames),
event groups (system state flags), binary/counting semaphores (ISR→task signalling),
software timers (blink, auto-lock delay). **No shared mutable globals** *(SRS-REL-002)*.

The watchdog task refreshes the IWDG **only** when every registered task has
checked in within its deadline — a classic supervisor pattern turning a task
hang into a controlled reset *(SRS-SAFETY-006)*.

---

## 5. State machines (Phase 3)
Four FSMs run on the shared table-driven `fsm` engine:
- **Lighting:** OFF → Parking → DRL → Low Beam → High Beam (guard: high needs low).
- **Indicator:** Idle → Left / Right → Hazard (hazard overrides).
- **Door:** Locked → Unlocked → Welcome → Auto-Lock (guard: no lock while open).
- **Power:** Sleep → Wake → Run → Shutdown.

A single engine + per-FSM transition tables keeps behaviour declarative,
inspectable, and unit-testable without hardware.

---

## 6. Communication architecture (Phase 2)
Frame: `[HDR][CMD][LEN][PAYLOAD...][CKSUM][FTR]`. The codec (byte framing +
checksum) is isolated from the dispatcher (command → handler). Both are pure
logic → fully unit-tested and shared, byte-for-byte, with the C# desktop tool
via the Interface Control Document (`BCM-ICD-001`). Details in Phase 2.

---

## 7. Build & target strategy

| Build | Toolchain | Purpose |
|-------|-----------|---------|
| **Target** | `arm-none-eabi-g++` (C++14) + ST HAL | Flashable `bcm_firmware.{elf,hex,bin}` |
| **Host / SIL** | native g++/MSVC | Run logic on PC, drive from WPF tool |
| **Unit tests** | GoogleTest/Catch2 on host | Per-module verification |

- Vendor code (CMSIS/HAL/FreeRTOS) is fetched via `make deps`, never committed.
- Application/service code avoids `#include`ing HAL directly; it talks to driver
  interfaces, so the same `.cpp` compiles for target and host. *(SRS-MNT-002)*

### 7.1 Memory budget (Phase 0 measured)
`text 3336 B · data 20 B · bss 1572 B` on 64 KB flash / 20 KB RAM — ~5 % flash,
~8 % RAM at bring-up, leaving ample headroom for the full stack *(SRS-PERF-003)*.

---

## 8. Directory structure
```
bcm-simulator/
├─ firmware/          # embedded target (this phase builds)
│  ├─ core/           # main, startup glue, ISRs, HAL conf, C++ runtime, syscalls
│  ├─ bsp/            # board_config.h, BSP facade, linker script
│  ├─ common/         # HAL-free shared types/utilities
│  ├─ drivers/        # driver layer            (Phase 1)
│  ├─ services/       # managers + FSM engine   (Phase 2-3)
│  ├─ app/            # composition + task bodies (Phase 4)
│  └─ vendor/         # fetched: STM32CubeF1, FreeRTOS-Kernel (gitignored)
├─ sil/               # host simulator          (Phase 5)
├─ tests/             # GoogleTest suites       (Phase 1+)
├─ desktop/           # C# WPF diagnostic tool  (Phase 6)
├─ docs/              # SRS, architecture, protocol, UML, test, manuals
└─ tools/             # helper scripts
```

---

## 9. Coding standards (enforced)
C++14, MISRA-inspired: `#pragma once`/guards, namespaces, `enum class`, no magic
numbers (central config), const-correctness, RAII, no dynamic allocation on
real-time paths, no exceptions/RTTI on target. Formatting via `.clang-format`.

---

## 10. Open architectural decisions (ADR log — seed)
| # | Decision | Rationale | Status |
|---|----------|-----------|--------|
| ADR-001 | UART transport (not CAN) | Cheap hardware, still exercises framing/diag patterns | Accepted |
| ADR-002 | C++14, no exceptions/RTTI on target | Determinism, size, MISRA alignment | Accepted |
| ADR-003 | Table-driven FSM engine (shared) | Declarative, testable, DRY across 4 machines | Accepted |
| ADR-004 | Logic host-compilable via driver interfaces | Enables SIL + unit tests | Accepted |
