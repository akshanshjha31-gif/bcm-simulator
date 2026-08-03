# Desktop Diagnostic Tool (Phase 6)

C# WPF application (.NET 8, MVVM, no external MVVM framework). Automotive-style
dark diagnostic UI in the spirit of a garage scan tool: connect over the serial
link, watch the vehicle live, drive outputs, and read fault codes. Speaks the
framed protocol defined in `docs/protocol/ICD.md` (BCM-ICD-001).

```
desktop/
├─ BcmDiagnosticTool/          the application
│  ├─ Protocol/                BCM-ICD-001 codec (CRC-8, encoder, parser)
│  ├─ Services/BcmClient.cs    transport + request/response correlation
│  ├─ ViewModels/              MVVM
│  ├─ Views/DarkTheme.xaml     styling
│  └─ MainWindow.xaml          dashboard
└─ BcmDiagnosticTool.Tests/    ICD conformance (xUnit)
```

## Running it

```powershell
tools\bcm.ps1 desktop                    # build and launch
tools\bcm.ps1 desktop -SilArgs test      # run the conformance tests
```

Or directly:

```powershell
$env:DOTNET_ROOT = "$env:USERPROFILE\.dotnet"
dotnet build desktop\BcmDiagnosticTool.sln -c Release
.\desktop\BcmDiagnosticTool\bin\Release\net8.0-windows\BcmDiagnosticTool.exe
```

`--connect COMx` opens the link on startup instead of waiting for a click.

> **`DOTNET_ROOT` matters.** The SDK here is installed per-user under
> `~/.dotnet`, which is not a machine-wide install, so the generated app host
> cannot locate `hostfxr.dll` by itself and the executable exits immediately
> with *"You must install .NET to run this application"*. `tools\bcm.ps1
> desktop` sets it for you.

## What it shows

| Panel | Contents |
|---|---|
| Connection | Port picker, link health, firmware version, frame statistics |
| Lamps | All 10 lamps live. **Click one to drive it** over the link |
| Inputs | The 6 switches, plus power state and battery level |
| Actions | Lock / unlock doors, clear DTCs |
| Fault codes | Active DTCs |
| Event log | Unsolicited `LOG_EVENT` frames, faults highlighted |

## Two behaviours that look like bugs but are not

**Lock can answer `NOT_PERMITTED`.** The BCM refuses to lock while a door is
open (SRS-DOOR-002). The tool reports the refusal rather than hiding it.

**Clicking the reverse lamp may leave it dark.** Host lamp requests are merged
*before* safety arbitration, so a diagnostic host can exercise a lamp but
cannot talk its way past a safety rule — the reverse lamp needs reverse gear
selected (SRS-REV-001).

## Protocol independence

`Protocol/` is written from **BCM-ICD-001**, not ported from the firmware.
`BcmDiagnosticTool.Tests` asserts the ICD's worked examples byte-for-byte, and
`tests/test_icd_examples.cpp` asserts the same bytes on the firmware side. Two
independent implementations agreeing on the documented bytes is the evidence
that the ICD is unambiguous — which is the reason the document exists.

Per BCM-ICD-001 §4.0 the client never assumes the next frame is its reply:
`LOG_EVENT` frames arrive asynchronously, so responses are matched on
`request | 0x80` and anything else is dispatched as an event.
