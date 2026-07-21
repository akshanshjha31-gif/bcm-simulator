# Service / Manager Layer (Phases 2-3)

HAL-free business logic — the part that is unit-tested and reused by the SIL.

Planned managers: `comm`, `lighting`, `door`, `horn`, `power`, `sensor`,
`fault`, `diag`, `config`, `logger`, plus a reusable table-driven `fsm` engine
shared by the Lighting, Indicator, Door and Power state machines.
