# UML Model
### Automotive Body Control Module (BCM) Simulator

| | |
|---|---|
| **Document ID** | BCM-UML-001 |
| **Version** | 1.0 |
| **Traces** | `BCM-SRS-001`, `BCM-ARC-001`, `BCM-ICD-001` |

Diagrams are written in Mermaid so they render directly on GitHub and stay
diffable in review — a PlantUML or Visio artefact would need a toolchain and
would drift from the code silently.

---

## 1. Package / layer view

Dependencies point downward only. This is the constraint the whole design
rests on: it is what allows every service to compile for the host.

```mermaid
graph TD
    APP["<b>app</b><br/>BcmApp · rtos_app · commands"]
    SVC["<b>services</b><br/>BcmCore · FSMs · arbiter · protocol · diagnostics"]
    DRV["<b>drivers</b><br/>gpio · button · led · uart · adc · timer · watchdog"]
    BSP["<b>bsp</b><br/>board_config.h — the only wiring map"]
    HAL["STM32 HAL + CMSIS + FreeRTOS<br/><i>fetched, not committed</i>"]
    HW["STM32F103C8 hardware"]

    SIL["<b>sil</b><br/>SilRunner · scenarios"]
    TST["<b>tests</b><br/>Catch2 host suite"]
    PC["<b>desktop</b><br/>WPF diagnostic tool"]

    APP --> SVC --> DRV --> BSP --> HAL --> HW
    APP --> BSP
    SIL -.drives.-> SVC
    TST -.tests.-> SVC
    TST -.tests.-> DRV
    PC -.BCM-ICD-001<br/>over UART.-> APP

    style SVC fill:#1B4D3E,stroke:#3FBF7F,color:#fff
    style SIL fill:#2A3A55,stroke:#4C8DFF,color:#fff
    style TST fill:#2A3A55,stroke:#4C8DFF,color:#fff
    style PC  fill:#4A3A20,stroke:#FFC24B,color:#fff
```

`sil` and `tests` attach to **services**, not to `app` — that is only possible
because nothing in the service layer includes a HAL header.

---

## 2. Class diagram — control core

`BcmCore` is the object both the firmware and the SIL harness drive.

```mermaid
classDiagram
    class BcmCore {
        -Config cfg_
        -Inputs inputs_
        -LampState lamps_
        +set_inputs(Inputs)
        +set_comms_lost(bool)
        +set_host_request(LampState, uint16)
        +step(uint16 dt_ms)
        +lamps() LampState
        +horn() bool
    }

    class LightingMgr {
        -Fsm~LightState~ light_
        -Fsm~IndState~ indicator_
        -Hysteresis dark_
        +next_mode()
        +update_ambient(uint16)
        +indicator_left()
        +indicator_hazard()
        +apply(LampState)
    }

    class DoorMgr {
        -Fsm~DoorState~ fsm_
        -SoftTimer welcome_
        -SoftTimer auto_lock_
        +request_lock()
        +request_unlock()
        +update(uint32)
        +take_lock_refused() bool
    }

    class PowerMgr {
        -Fsm~PowerState~ fsm_
        -Hysteresis battery_ok_
        +ignition_on()
        +update_battery(uint16)
        +load_shed() bool
        +lamps_permitted() bool
    }

    class HornMgr {
        +set_continuous(bool)
        +chirp(uint8)
        +update(uint32) bool
    }

    class LampArbiter {
        <<static>>
        +arbitrate(LampState, ArbiterInputs)$
        +is_essential(LampId) bool$
    }

    class FaultMgr {
        +set(Dtc)
        +clear(Dtc)
        +active_codes(uint8*, uint8) uint8
    }

    class Logger {
        +log(uint32, LogEvent, uint8)
        +pop(LogRecord) bool
    }

    class Fsm~StateT, EventT, CtxT~ {
        -Transition* table_
        -StateT state_
        +dispatch(EventT, CtxT) bool
        +blocked_count() uint32
    }

    BcmCore *-- LightingMgr
    BcmCore *-- DoorMgr
    BcmCore *-- PowerMgr
    BcmCore *-- HornMgr
    BcmCore *-- FaultMgr
    BcmCore *-- Logger
    BcmCore ..> LampArbiter : applies last

    LightingMgr *-- Fsm
    DoorMgr     *-- Fsm
    PowerMgr    *-- Fsm
```

Every state machine shares one engine and differs only in its transition
table (ADR-003), which keeps behaviour declarative rather than buried in
conditionals.

---

## 3. Class diagram — driver layer

The recurring shape: hardware-free logic on the left, a thin binding on the
right. The left column is what the 189 unit tests exercise.

```mermaid
classDiagram
    direction LR

    class Debouncer {
        +update(bool raw, uint16 dt) bool
        +state() bool
        +rising() bool
    }
    class Blinker {
        +set_blink(uint16 on, uint16 off, bool start_on)
        +update(uint16 dt) bool
    }
    class ExpFilter {
        +update(uint16) uint16
    }
    class Hysteresis {
        +update(uint16) bool
    }
    class RingBuffer~T, N~ {
        +push(T) bool
        +pop(T) bool
    }
    class SoftTimer {
        +start(uint32, bool periodic)
        +update(uint32) bool
    }

    class GpioPin {
        -GPIO_TypeDef* port_
        -bool active_low_
        +write(bool asserted)
        +read() bool
    }
    class Button {
        +update(uint16 dt)
        +pressed() bool
        +just_pressed() bool
    }
    class Led {
        +on()
        +blink(uint16, uint16)
        +update(uint16 dt)
    }
    class Uart {
        <<static>>
        +init(UartConfig) bool
        +write(uint8*, uint16) bool
        +on_irq()$
    }
    class Adc {
        <<static>>
        +read_raw(uint32) uint16
    }
    class Watchdog {
        <<static>>
        +start(uint32) bool
        +refresh()
    }

    Button *-- Debouncer
    Button *-- GpioPin
    Led    *-- Blinker
    Led    *-- GpioPin
    Uart   *-- RingBuffer
    Adc    <.. ExpFilter : AnalogInput

    note for Debouncer "HAL-free — host tested"
    note for Uart "HAL-bound — target only"
```

---

## 4. State machines

### 4.1 Lighting (SRS-LIGHT-001, SRS-LIGHT-003)

```mermaid
stateDiagram-v2
    [*] --> Off
    Off --> Parking : ModeNext
    Parking --> Drl : ModeNext
    Drl --> LowBeam : ModeNext
    LowBeam --> HighBeam : ModeNext
    HighBeam --> Off : ModeNext

    Off --> LowBeam : AutoOn<br/>[not manual_override]
    Parking --> LowBeam : AutoOn<br/>[not manual_override]
    Drl --> LowBeam : AutoOn<br/>[not manual_override]
    LowBeam --> Off : AutoOff<br/>[auto owns lamps]

    note right of HighBeam
        High beam never runs alone —
        low beam stays lit beneath it
    end note
```

The `manual_override` guard is what stops the ambient sensor overriding a
driver who has chosen a mode by hand.

### 4.2 Indicator (SRS-IND-003)

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Left : LeftPressed
    Idle --> Right : RightPressed
    Left --> Idle : LeftPressed
    Right --> Idle : RightPressed
    Left --> Right : RightPressed
    Right --> Left : LeftPressed

    Idle --> Hazard : HazardPressed
    Left --> Hazard : HazardPressed
    Right --> Hazard : HazardPressed
    Hazard --> Idle : HazardPressed

    note right of Hazard
        Hazard wins from ANY state.
        There is deliberately no
        Hazard → Left/Right rule, so
        the FSM refuses direction
        requests while it is latched.
    end note
```

### 4.3 Door (SRS-DOOR-002 — the load-bearing guard)

```mermaid
stateDiagram-v2
    [*] --> Locked
    Locked --> Welcome : UnlockRequest
    Welcome --> Unlocked : WelcomeExpired
    Welcome --> Locked : LockRequest<br/>[no door open]
    Unlocked --> Locked : LockRequest<br/>[no door open]
    Unlocked --> Locked : AutoLockElapsed<br/>[no door open]

    note left of Locked
        The guard is on EVERY locking
        transition, manual and automatic.
        A rule enforced only on the manual
        path is not enforced at all.
    end note

    note right of Welcome
        Unlock has NO guard, by design:
        refusing to unlock could trap
        an occupant.
    end note
```

### 4.4 Power (SRS-PWR-001)

```mermaid
stateDiagram-v2
    [*] --> Sleep
    Sleep --> Wake : IgnitionOn
    Wake --> Run : WakeComplete
    Wake --> Shutdown : IgnitionOff
    Run --> Shutdown : IgnitionOff
    Run --> Shutdown : InactivityElapsed
    Shutdown --> Sleep : ShutdownComplete
    Shutdown --> Run : IgnitionOn

    note right of Shutdown
        Ignition during shutdown aborts it —
        the driver's intent wins.
    end note
```

---

## 5. Sequence diagrams

### 5.1 One control cycle (5 ms)

Arbitration is deliberately the last step before the pins.

```mermaid
sequenceDiagram
    participant T as Control task
    participant A as BcmApp
    participant C as BcmCore
    participant M as Managers
    participant Ar as LampArbiter
    participant D as Led / GpioPin

    T->>A: step(dt)
    A->>A: gather_inputs() — debounce pins
    A->>C: set_inputs(Inputs)
    A->>C: set_host_request(lamps, mask)
    A->>C: step(dt)

    C->>M: edges → FSM events
    C->>M: update(dt)
    M-->>C: apply(wanted)
    C->>C: merge host requests
    C->>Ar: arbitrate(wanted, arb)
    Note over Ar: brake overrides · reverse gated<br/>load shed · sleep · comms lost
    Ar-->>C: arbitrated LampState

    A->>D: drive pins
    A->>A: comm_.poll(dt)
```

### 5.2 A diagnostic request (BCM-ICD-001)

```mermaid
sequenceDiagram
    participant H as WPF tool
    participant U as drv_uart (ISR)
    participant P as FrameParser
    participant Di as Dispatcher
    participant Hn as Handler

    H->>U: AA 04 02 07 01 E2 55
    U->>U: RXNE → ring buffer
    U->>P: feed(byte) ×7
    P->>P: verify CRC-8 + footer
    P-->>Di: Frame{cmd=0x04, payload=[07,01]}

    Di->>Di: look up 0x04 in the table
    alt unknown or short
        Di-->>H: STATUS = UNKNOWN_CMD / BAD_LENGTH
    else accepted
        Di->>Hn: handle_set_lamp()
        Hn-->>Di: record host request
        Di-->>H: AA 84 01 00 B5 55
    end

    Note over Hn: The request is merged BEFORE<br/>arbitration, so a host can exercise<br/>a lamp but not defeat a safety rule
```

### 5.3 Watchdog supervision (SRS-SAFETY-006)

```mermaid
sequenceDiagram
    participant C as Control
    participant S as Sensor
    participant L as Logger
    participant Mo as Monitor
    participant W as Watchdog task
    participant I as IWDG

    C->>W: set bit 0
    S->>W: set bit 1
    L->>W: set bit 2
    Mo->>W: set bit 3

    W->>W: xEventGroupWaitBits(ALL, clear-on-exit)
    alt all checked in AND health not Failed
        W->>I: refresh()
    else a task hung, or health Failed
        Note over W,I: deliberately NOT refreshed —<br/>the IWDG expires and the MCU<br/>resets into a defined state
    end
```

---

## 6. Concurrency view (FreeRTOS)

```mermaid
graph LR
    subgraph P5["prio 5"]
        WD[Watchdog<br/>200 ms]
    end
    subgraph P4["prio 4"]
        MO[Monitor<br/>100 ms]
    end
    subgraph P3["prio 3"]
        CT[Control<br/>5 ms]
    end
    subgraph P2["prio 2"]
        SE[Sensor<br/>20 ms]
    end
    subgraph P1["prio 1"]
        LG[Logger<br/>queue]
    end

    EG(("event group<br/>check-in"))
    LQ(("queue<br/>log records"))
    SEM(("semaphore<br/>UART RX"))

    CT --> EG
    SE --> EG
    LG --> EG
    MO --> EG
    EG --> WD

    CT --> LQ --> LG
    SEM --> CT

    SE -.->|volatile uint16| CT

    style WD fill:#4A2020,stroke:#E5484D,color:#fff
    style CT fill:#1B4D3E,stroke:#3FBF7F,color:#fff
```

**Five tasks, not the nine in the architecture document.** Lighting, Door and
Power are merged into Control so every state machine has a single owner and
the design needs no mutex around any FSM. On a single-core M3 splitting them
would buy nothing and would reintroduce exactly the shared mutable state that
SRS-REL-002 forbids.

The battery reading crosses from Sensor to Control as a single `volatile
uint16_t` — an aligned 16-bit load/store is atomic on Cortex-M3, so no lock is
required for that either.

---

## 7. Deployment view

```mermaid
graph TB
    subgraph PC["Development PC — Windows"]
        IDE["VS Code / Make"]
        WPF["BcmDiagnosticTool.exe<br/><i>.NET 8 WPF</i>"]
        SILX["bcm_sil.exe<br/><i>scenarios</i>"]
        UT["bcm_tests.exe<br/><i>Catch2</i>"]
        QEMU["QEMU 2.8<br/><i>no board needed</i>"]
    end

    subgraph BOARD["STM32F103C8 — Blue Pill"]
        FW["bcm_firmware.elf<br/>24.6 KB flash · 8.5 KB RAM"]
    end

    subgraph IO["Breadboard"]
        LEDS["10 LEDs"]
        BTN["6 buttons"]
        POT["potentiometer"]
        BUZ["buzzer"]
    end

    STL["ST-Link V2"]
    UART["USB-UART adapter"]

    IDE -->|build| FW
    STL -->|SWD: flash + debug| FW
    UART -->|"115200 8N1<br/>PB6/PB7"| FW
    WPF -->|BCM-ICD-001| UART
    FW --> LEDS
    BTN --> FW
    POT --> FW
    FW --> BUZ

    SILX -.->|same services| FW
    UT   -.->|same services| FW
    QEMU -.->|runs the ELF| FW

    style FW fill:#1B4D3E,stroke:#3FBF7F,color:#fff
```

> **USART1 is remapped to PB6/PB7.** The default pins are PA9/PA10, but PA9
> drives the door-lock lamp on this board. USART2 collides with the indicator
> switches and USART3 with the beam lamps, so the AFIO remap was the only
> option. Note that the **ROM bootloader is fixed to PA9/PA10** and cannot be
> remapped — relevant only if flashing over serial instead of SWD.

---

## 8. Revision history

| Version | Change |
|---|---|
| 1.0 | Initial issue — layer, class, state, sequence, concurrency and deployment views |
