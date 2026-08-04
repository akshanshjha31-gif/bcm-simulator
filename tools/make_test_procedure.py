#!/usr/bin/env python3
"""
Generate the BCM Simulator acceptance test procedure as a Word document.

Kept as a script rather than a hand-edited .docx so the procedure stays
reviewable in a diff and can be regenerated whenever the project changes -
a binary document that only one person can edit rots quickly.

    python tools/make_test_procedure.py

Output: docs/test/BCM-TestProcedure.docx
"""

from pathlib import Path

from docx import Document
from docx.enum.section import WD_ORIENT
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor

ACCENT = RGBColor(0x1F, 0x4E, 0x79)
MUTED = RGBColor(0x59, 0x59, 0x59)
DANGER = RGBColor(0xC0, 0x00, 0x00)
OK = RGBColor(0x1E, 0x7A, 0x3C)

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "test" / "BCM-TestProcedure.docx"


# --------------------------------------------------------------------------
# helpers
# --------------------------------------------------------------------------

def shade(cell, hex_colour):
    """Fill a table cell - python-docx has no API for this."""
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), hex_colour)
    tc_pr.append(shd)


def cell_text(cell, text, *, bold=False, size=9, colour=None, mono=False):
    cell.text = ""
    p = cell.paragraphs[0]
    run = p.add_run(text)
    run.bold = bold
    run.font.size = Pt(size)
    run.font.name = "Consolas" if mono else "Calibri"
    if colour:
        run.font.color.rgb = colour


def para(doc, text="", *, size=10.5, bold=False, italic=False, colour=None,
         space_after=6, mono=False, align=None):
    p = doc.add_paragraph()
    run = p.add_run(text)
    run.bold = bold
    run.italic = italic
    run.font.size = Pt(size)
    run.font.name = "Consolas" if mono else "Calibri"
    if colour:
        run.font.color.rgb = colour
    p.paragraph_format.space_after = Pt(space_after)
    if align:
        p.alignment = align
    return p


def bullet(doc, text, *, size=10.5, bold=False):
    p = doc.add_paragraph(style="List Bullet")
    run = p.add_run(text)
    run.bold = bold
    run.font.size = Pt(size)
    p.paragraph_format.space_after = Pt(3)
    return p


def code_block(doc, lines):
    """Shaded monospace block, for commands and expected output."""
    table = doc.add_table(rows=1, cols=1)
    table.alignment = WD_TABLE_ALIGNMENT.LEFT
    cell = table.cell(0, 0)
    shade(cell, "F2F4F7")
    cell.text = ""
    for i, line in enumerate(lines):
        p = cell.paragraphs[0] if i == 0 else cell.add_paragraph()
        run = p.add_run(line)
        run.font.name = "Consolas"
        run.font.size = Pt(9)
        p.paragraph_format.space_after = Pt(0)
    doc.add_paragraph().paragraph_format.space_after = Pt(4)
    return table


def callout(doc, title, body, colour):
    """A boxed note that the reader cannot skim past."""
    table = doc.add_table(rows=1, cols=1)
    cell = table.cell(0, 0)
    shade(cell, "FFF4E5" if colour == DANGER else "EAF3EA")
    cell.text = ""
    p = cell.paragraphs[0]
    r = p.add_run(title)
    r.bold = True
    r.font.size = Pt(10)
    r.font.color.rgb = colour
    p2 = cell.add_paragraph()
    r2 = p2.add_run(body)
    r2.font.size = Pt(10)
    p2.paragraph_format.space_after = Pt(0)
    doc.add_paragraph().paragraph_format.space_after = Pt(4)


def heading(doc, text, level):
    h = doc.add_heading(text, level=level)
    for run in h.runs:
        run.font.color.rgb = ACCENT
    return h


def test_case(doc, tc_id, title, requirement, precondition, steps, expected):
    """One numbered test case with a Pass/Fail box the tester fills in."""
    heading(doc, f"{tc_id} — {title}", 3)

    meta = doc.add_table(rows=2, cols=2)
    meta.style = "Table Grid"
    meta.columns[0].width = Inches(1.3)
    meta.columns[1].width = Inches(5.4)
    cell_text(meta.cell(0, 0), "Requirement", bold=True)
    cell_text(meta.cell(0, 1), requirement)
    cell_text(meta.cell(1, 0), "Precondition", bold=True)
    cell_text(meta.cell(1, 1), precondition)
    doc.add_paragraph().paragraph_format.space_after = Pt(2)

    t = doc.add_table(rows=1, cols=4)
    t.style = "Table Grid"
    widths = [Inches(0.4), Inches(2.9), Inches(2.7), Inches(0.75)]
    hdr = t.rows[0]
    for i, name in enumerate(["#", "Action", "Expected result", "Pass?"]):
        cell_text(hdr.cells[i], name, bold=True)
        shade(hdr.cells[i], "1F4E79")
        hdr.cells[i].paragraphs[0].runs[0].font.color.rgb = RGBColor(
            0xFF, 0xFF, 0xFF)
        t.columns[i].width = widths[i]

    for n, (action, result) in enumerate(zip(steps, expected), start=1):
        row = t.add_row()
        cell_text(row.cells[0], str(n))
        cell_text(row.cells[1], action)
        cell_text(row.cells[2], result)
        cell_text(row.cells[3], "☐ P   ☐ F")
        for i, w in enumerate(widths):
            row.cells[i].width = w

    doc.add_paragraph().paragraph_format.space_after = Pt(10)


# --------------------------------------------------------------------------
# document
# --------------------------------------------------------------------------

def build():
    doc = Document()

    style = doc.styles["Normal"]
    style.font.name = "Calibri"
    style.font.size = Pt(10.5)

    for section in doc.sections:
        section.top_margin = Inches(0.7)
        section.bottom_margin = Inches(0.7)
        section.left_margin = Inches(0.8)
        section.right_margin = Inches(0.8)

    # ---- Cover -----------------------------------------------------------
    t = doc.add_heading("Acceptance Test Procedure", level=0)
    for run in t.runs:
        run.font.color.rgb = ACCENT
    para(doc, "Automotive Body Control Module (BCM) Simulator",
         size=15, bold=True, colour=MUTED, space_after=14)

    info = doc.add_table(rows=6, cols=2)
    info.style = "Table Grid"
    rows = [
        ("Document ID", "BCM-ATP-001"),
        ("Version", "1.0"),
        ("Software version", "1.0.0"),
        ("Target", "STM32F103C8T6 'Blue Pill', 72 MHz Cortex-M3"),
        ("Tester", "________________________________"),
        ("Date", "________________________________"),
    ]
    for i, (k, v) in enumerate(rows):
        cell_text(info.cell(i, 0), k, bold=True)
        cell_text(info.cell(i, 1), v)
        info.columns[0].width = Inches(1.6)
        info.columns[1].width = Inches(5.1)

    doc.add_paragraph()
    heading(doc, "How to use this document", 2)
    para(doc,
         "Work through the parts in order. Each test case has numbered steps, "
         "the exact result to expect, and a Pass/Fail box to tick. If a step "
         "fails, stop and check Part F (Troubleshooting) before continuing - "
         "later tests usually depend on earlier ones.")
    bullet(doc, "Part A runs entirely on the PC. No board required.")
    bullet(doc, "Part B connects and programmes the hardware.")
    bullet(doc, "Part C is the hands-on functional test.")
    bullet(doc, "Part D covers the safety-critical behaviour.")
    bullet(doc, "Part E uses the Windows diagnostic tool.")
    bullet(doc, "Part F is troubleshooting; Part G is the sign-off sheet.")

    doc.add_paragraph()
    callout(doc, "READ THIS FIRST — the most common mistake",
            "The BCM starts in SLEEP, and a sleeping vehicle deliberately keeps "
            "every lamp off except the door-lock indicator. If you press the "
            "indicator or hazard buttons before waking it, NOTHING will light "
            "and the hardware will look broken when it is working correctly. "
            "Always run TC-C-01 (wake the BCM) first.\n\n"
            "The ignition button is dual-function: a SHORT tap (under 0.8 s) "
            "toggles power on and off, while HOLDING it for 0.8 s or more "
            "changes the lighting mode instead and does not toggle power. If "
            "you hold it too long, the BCM never wakes.",
            DANGER)

    doc.add_page_break()

    # ---- Prerequisites ---------------------------------------------------
    heading(doc, "Prerequisites", 1)

    heading(doc, "Equipment", 2)
    eq = doc.add_table(rows=1, cols=3)
    eq.style = "Table Grid"
    for i, name in enumerate(["Item", "Notes", "Present?"]):
        cell_text(eq.cell(0, i), name, bold=True)
        shade(eq.cell(0, i), "1F4E79")
        eq.cell(0, i).paragraphs[0].runs[0].font.color.rgb = RGBColor(
            0xFF, 0xFF, 0xFF)
    for item, note in [
        ("STM32F103C8 'Blue Pill'", "Power LED must light when powered"),
        ("ST-Link V2 programmer", "For flashing and SWD debug"),
        ("USB-UART adapter", "3.3 V logic. NOT the same as the ST-Link"),
        ("10 LEDs + 220 Ω–1 kΩ resistors", "10 kΩ makes an LED invisible"),
        ("6 push buttons + 10 kΩ resistors", "Wired +3.3 V → button → GPIO → 10 kΩ → GND"),
        ("Potentiometer", "Simulates battery voltage, on PA6"),
        ("Buzzer", "On PB8"),
        ("Windows PC", "With this repository checked out"),
    ]:
        r = eq.add_row()
        cell_text(r.cells[0], item)
        cell_text(r.cells[1], note)
        cell_text(r.cells[2], "☐")

    doc.add_paragraph()
    heading(doc, "Wiring", 2)
    code_block(doc, [
        "ST-Link            Blue Pill        UART adapter        Blue Pill",
        "-------            ---------        ------------        ---------",
        "3.3V      ---->    3V3              RX      <-----       PB6  (BCM TX)",
        "GND       ---->    GND              TX      ----->       PB7  (BCM RX)",
        "SWDIO     ---->    DIO  (PA13)      GND     <---->       GND",
        "SWCLK     ---->    CLK  (PA14)",
    ])
    para(doc, "RX and TX cross over. Do not connect the UART adapter's power "
              "pin if the board is already powered.", italic=True, size=10)

    doc.add_paragraph()
    heading(doc, "I/O map", 2)
    io = doc.add_table(rows=1, cols=4)
    io.style = "Table Grid"
    for i, name in enumerate(["Pin", "Function", "Pin", "Function"]):
        cell_text(io.cell(0, i), name, bold=True)
        shade(io.cell(0, i), "1F4E79")
        io.cell(0, i).paragraphs[0].runs[0].font.color.rgb = RGBColor(
            0xFF, 0xFF, 0xFF)
    pins = [
        ("PA0", "Ignition button", "PB0", "Ignition lamp"),
        ("PA1", "Left indicator button", "PB1", "DRL lamp"),
        ("PA2", "Right indicator button", "PB8", "Buzzer"),
        ("PA3", "Hazard button", "PB10", "Low beam"),
        ("PA4", "Brake button", "PB11", "High beam"),
        ("PA5", "Door lock button", "PB12", "Left indicator lamp"),
        ("PA6", "Potentiometer (ADC)", "PB13", "Right indicator lamp"),
        ("PA8", "Reverse lamp", "PB14", "Hazard lamp"),
        ("PA9", "Door lock lamp", "PB15", "Brake lamp"),
        ("PA13/PA14", "SWD — keep free", "PB6/PB7", "Diagnostic UART"),
    ]
    for a, b, c, d in pins:
        r = io.add_row()
        for i, v in enumerate([a, b, c, d]):
            cell_text(r.cells[i], v)

    doc.add_page_break()

    # ---- Part A ----------------------------------------------------------
    heading(doc, "Part A — Software tests (no hardware needed)", 1)
    para(doc, "Run these from the repository root in PowerShell. Every command "
              "returns a non-zero exit code on failure, so they can also be "
              "used in continuous integration.")

    test_case(
        doc, "TC-A-01", "Unit tests", "SRS-MNT-002 and all module requirements",
        "Repository checked out. No hardware required.",
        ["Open PowerShell in the repository root.",
         "Run:  tools\\bcm.ps1 test",
         "Read the final two lines."],
        ["PowerShell prompt is at C:\\...\\bcm-simulator",
         "The suite builds, then runs.",
         "'All tests passed (3630 assertions in 189 test cases)' "
         "followed by 'PASS: all unit tests green.'"],
    )

    test_case(
        doc, "TC-A-02", "Software-in-the-Loop scenarios",
        "SRS-LIGHT-*, SRS-IND-*, SRS-DOOR-*, SRS-PWR-*, SRS-SAFETY-*",
        "TC-A-01 passed.",
        ["Run:  tools\\bcm.ps1 sil",
         "Check every scenario line reads PASS.",
         "Optional:  tools\\bcm.ps1 sil -SilArgs --verbose",
         "Optional:  tools\\bcm.ps1 sil -SilArgs door-open-guard"],
        ["15 scenarios listed, each marked PASS.",
         "'scenarios: 15  passed: 15  failed: 0' and 'RESULT: PASS'",
         "Each individual check is listed as well.",
         "Runs that one scenario only: 1 passed, 5 checks."],
    )

    test_case(
        doc, "TC-A-03", "Protocol conformance (C# vs firmware)",
        "BCM-ICD-001",
        "None.",
        ["Run:  tools\\bcm.ps1 desktop -SilArgs test",
         "Read the summary line."],
        ["The solution builds with 0 errors.",
         "'Passed! - Failed: 0, Passed: 17, Skipped: 0, Total: 17'"],
    )

    test_case(
        doc, "TC-A-04", "Emulated run (QEMU)", "SRS-PERF-003",
        "None.",
        ["Run:  tools\\bcm.ps1 run",
         "Observe the console for a few seconds.",
         "Press Ctrl+C to stop."],
        ["QEMU reports board 'BluePill' / device STM32F103C8.",
         "Alternating '[led:red on]' and '[led:red off]' lines.",
         "Returns to the prompt."],
    )
    callout(doc, "Note on QEMU timing",
            "QEMU is not cycle-accurate: a 500 ms delay measures around 722 ms "
            "there. Use it to check logic, never to measure timing. The real "
            "hardware is exact.", OK)

    doc.add_page_break()

    # ---- Part B ----------------------------------------------------------
    heading(doc, "Part B — Hardware setup and programming", 1)

    test_case(
        doc, "TC-B-01", "Power and connections", "—",
        "Board wired as shown in Prerequisites.",
        ["Connect the ST-Link to the PC.",
         "Look at the board's red power LED.",
         "Run:  tools\\bcm.ps1 ports"],
        ["Windows detects the ST-Link (no driver warning).",
         "The power LED is LIT. If dark, stop - see Part F.",
         "The UART adapter's COM port is listed, e.g. COM13."],
    )

    test_case(
        doc, "TC-B-02", "Programme the firmware", "SRS-PERF-003",
        "TC-B-01 passed.",
        ["Run:  tools\\bcm.ps1 flash",
         "Read the size line.",
         "Read the final lines."],
        ["Builds, then connects over SWD.",
         "text ≈ 24564, data ≈ 124, bss ≈ 8456 (38 % flash, 43 % RAM).",
         "'** Verified OK **' and '==> Flash OK.'"],
    )

    test_case(
        doc, "TC-B-03", "Confirm the CPU is executing", "SRS-SAFETY-006",
        "TC-B-02 passed.",
        ["Run:  tools\\bcm.ps1 verify",
         "Look at the small LED on the Blue Pill itself."],
        ["'PASS: PC13 is toggling - firmware is running.' This reads the "
         "output register live over SWD, so it proves the processor is "
         "executing, not merely that bytes reached flash.",
         "The on-board LED blinks about once per second."],
    )

    test_case(
        doc, "TC-B-04", "Diagnostic protocol over the real link",
        "SRS-COM-001, SRS-COM-002, SRS-COM-003",
        "TC-B-03 passed. UART adapter connected to PB6/PB7.",
        ["Close the diagnostic tool if it is open (it holds the port).",
         "Run:  tools\\bcm.ps1 talk",
         "Check all six lines."],
        ["Only one program can use the COM port at a time.",
         "Six checks run.",
         "All six read [ OK ], ending 'PASS: 6/6 protocol checks OK.' "
         "GET_BATTERY shows a percentage that follows the potentiometer."],
    )

    doc.add_page_break()

    # ---- Part C ----------------------------------------------------------
    heading(doc, "Part C — Functional test (hands-on)", 1)
    callout(doc, "Do TC-C-01 first",
            "Every test below assumes the BCM is awake. If you skip TC-C-01 "
            "the lamps stay off by design and everything will look broken.",
            DANGER)

    test_case(
        doc, "TC-C-01", "Wake the BCM", "SRS-PWR-001, SRS-PWR-003",
        "TC-B-03 passed. Board powered.",
        ["Before touching anything, note which LEDs are lit.",
         "TAP the ignition button (PA0) - press and release within 0.8 s.",
         "Observe the Ignition lamp (PB0).",
         "TAP ignition again.",
         "TAP once more to wake it for the remaining tests."],
        ["Only the Door Lock lamp (PA9) is lit. All others off - this is "
         "correct: a sleeping vehicle keeps its lamps off.",
         "A short tap, not a long hold.",
         "The Ignition lamp comes ON. The BCM is now in Run.",
         "The Ignition lamp goes OFF - back to Sleep.",
         "Ignition lamp ON again."],
    )

    test_case(
        doc, "TC-C-02", "Lighting mode cycle", "SRS-LIGHT-001",
        "TC-C-01 passed, BCM awake.",
        ["HOLD the ignition button for at least 0.8 s, then release.",
         "Repeat the hold.",
         "Repeat the hold.",
         "Repeat the hold.",
         "Repeat the hold."],
        ["DRL lamp (PB1) lights - Parking.",
         "DRL stays lit - DRL mode.",
         "Low beam (PB10) lights as well.",
         "High beam (PB11) lights too, WITH low beam still on. High beam "
         "must never run alone.",
         "All lighting lamps go out - back to Off."],
    )

    test_case(
        doc, "TC-C-03", "Indicators", "SRS-IND-001, SRS-IND-004",
        "BCM awake.",
        ["Tap the left indicator button (PA1).",
         "Count the flashes over 10 seconds.",
         "Tap the left button again.",
         "Tap the right button (PA2).",
         "Tap the left button while the right is flashing."],
        ["Left indicator lamp (PB12) FLASHES, starting lit. The right lamp "
         "stays off.",
         "About 15 flashes - roughly 1.5 Hz.",
         "Flashing stops.",
         "Right lamp (PB13) flashes.",
         "The right lamp stops and the left starts - direct changeover."],
    )

    test_case(
        doc, "TC-C-04", "Horn and door locking",
        "SRS-DOOR-001, SRS-HORN-002",
        "BCM awake.",
        ["Note the Door Lock lamp (PA9) state.",
         "Tap the door lock button (PA5).",
         "Listen to the buzzer.",
         "Tap the door lock button again."],
        ["It is lit - doors start locked.",
         "The lamp goes out - unlocked. The DRL lamp lights briefly "
         "(welcome lighting) then goes out.",
         "Two short chirps on unlock; one chirp on lock.",
         "The Door Lock lamp comes back on - locked."],
    )

    test_case(
        doc, "TC-C-05", "Battery reading", "SRS-SENS-001",
        "BCM awake. tools\\bcm.ps1 talk available.",
        ["Turn the potentiometer fully anti-clockwise.",
         "Run:  tools\\bcm.ps1 talk",
         "Turn the potentiometer fully clockwise.",
         "Run tools\\bcm.ps1 talk again."],
        ["—",
         "GET_BATTERY reports a low percentage, near 0 %.",
         "—",
         "GET_BATTERY reports a high percentage, near 100 %. The reading "
         "tracks the knob."],
    )

    doc.add_page_break()

    # ---- Part D ----------------------------------------------------------
    heading(doc, "Part D — Safety tests", 1)
    para(doc, "These are the behaviours that make this a Body Control Module "
              "rather than a lamp driver. Each one must pass.", bold=True)

    test_case(
        doc, "TC-D-01", "Brake lamp has the highest priority",
        "SRS-BRK-001, SRS-SAFETY-003",
        "BCM awake.",
        ["Press and hold the brake button (PA4).",
         "While still holding brake, tap hazard.",
         "Release the brake button.",
         "Put the BCM to sleep (tap ignition), then press brake."],
        ["Brake lamp (PB15) lights immediately.",
         "The hazards flash AND the brake lamp stays lit. Brake is never "
         "displaced by another function.",
         "The brake lamp goes out - it follows the pedal exactly.",
         "The brake lamp STILL lights, even asleep. Brake overrides "
         "everything, including the sleep blanking."],
    )

    test_case(
        doc, "TC-D-02", "Hazard overrides the indicators", "SRS-IND-003",
        "BCM awake, indicators idle.",
        ["Tap the left indicator button.",
         "Tap the hazard button (PA3).",
         "Tap the left indicator button.",
         "Tap the right indicator button.",
         "Tap hazard again."],
        ["Left lamp flashes.",
         "BOTH indicator lamps and the hazard lamp (PB14) now flash - "
         "hazard has taken over.",
         "Nothing changes. Hazard cannot be pre-empted.",
         "Nothing changes.",
         "Hazard stops; the indicators are usable again."],
    )

    test_case(
        doc, "TC-D-03", "Load shedding protects the safety lamps",
        "SRS-PWR-004, SRS-SAFETY-003",
        "BCM awake. Potentiometer turned up.",
        ["Hold ignition long-presses until Low beam and DRL are lit.",
         "Tap the left indicator so it is flashing.",
         "Press and hold the brake button.",
         "SLOWLY turn the potentiometer down while watching the lamps.",
         "Turn the potentiometer back up."],
        ["DRL and Low beam are lit.",
         "Left indicator flashing.",
         "Brake lamp lit.",
         "DRL and High beam GO OUT, but the brake lamp and the indicator "
         "KEEP working. This is the key result: a failing battery never "
         "removes the lamps other road users depend on.",
         "The shed lamps return."],
    )

    test_case(
        doc, "TC-D-04", "Sleep blanks the lamps but keeps the lock indicator",
        "SRS-PWR-003",
        "BCM awake with several lamps lit.",
        ["Turn on Low beam via ignition long-presses.",
         "Tap ignition briefly to shut down.",
         "Wait 1 second and inspect every LED."],
        ["Low beam and DRL lit.",
         "The BCM shuts down.",
         "Every lamp is off EXCEPT the Door Lock lamp, which stays lit so a "
         "parked vehicle still shows it is secured."],
    )

    test_case(
        doc, "TC-D-05", "Reverse lamp is gated on gear",
        "SRS-REV-001, SRS-SAFETY-005",
        "BCM awake. Diagnostic tool connected (Part E) or talk available.",
        ["In the diagnostic tool, click the 'Reverse' lamp button.",
         "Observe the physical Reverse LED (PA8)."],
        ["The command is accepted with status OK.",
         "The LED stays DARK. No reverse-gear switch is wired, so the "
         "arbiter refuses it. A diagnostic host can exercise a lamp but "
         "cannot override a safety rule. This is a PASS."],
    )

    doc.add_page_break()

    # ---- Part E ----------------------------------------------------------
    heading(doc, "Part E — Diagnostic tool (Windows GUI)", 1)

    test_case(
        doc, "TC-E-01", "Launch and connect", "BCM-ICD-001",
        "TC-B-04 passed. 'tools\\bcm.ps1 talk' is NOT running.",
        ["Run:  tools\\bcm.ps1 desktop",
         "Select the COM port in the drop-down.",
         "Click Connect.",
         "Read the Firmware field."],
        ["A window titled 'BCM Diagnostic Tool' opens.",
         "Your adapter's port, e.g. COM13.",
         "The status dot turns GREEN.",
         "It shows v1.0.0."],
    )

    test_case(
        doc, "TC-E-02", "Live monitoring", "SRS-COM-003, SRS-DIAG-003",
        "TC-E-01 passed.",
        ["Press and hold each button on the board in turn.",
         "Turn the potentiometer.",
         "Tap ignition and watch the Power state field.",
         "Press several buttons and watch the Event log."],
        ["The matching indicator in the Inputs panel lights while held.",
         "The Battery bar and percentage follow the knob.",
         "It changes Sleep → Wake → Run.",
         "Entries appear with timestamps; faults appear in red."],
    )

    test_case(
        doc, "TC-E-03", "Driving outputs and reading faults",
        "SRS-COM-003, SRS-DIAG-002",
        "TC-E-01 passed, BCM awake.",
        ["Click the 'DRL' lamp button in the Lamps panel.",
         "Click it again.",
         "Click 'Lock doors'.",
         "Click 'Clear DTCs'.",
         "Turn the potentiometer fully down and wait 2 seconds."],
        ["The physical DRL LED lights.",
         "It goes out.",
         "The doors lock, the buzzer chirps, the Door Lock LED lights.",
         "The fault list empties.",
         "'0x01 Battery low' appears in the fault list."],
    )

    doc.add_page_break()

    # ---- Part F ----------------------------------------------------------
    heading(doc, "Part F — Troubleshooting", 1)
    tb = doc.add_table(rows=1, cols=3)
    tb.style = "Table Grid"
    for i, name in enumerate(["Symptom", "Most likely cause", "Action"]):
        cell_text(tb.cell(0, i), name, bold=True)
        shade(tb.cell(0, i), "1F4E79")
        tb.cell(0, i).paragraphs[0].runs[0].font.color.rgb = RGBColor(
            0xFF, 0xFF, 0xFF)
    issues = [
        ("No LEDs respond to any button, but the GUI shows the button presses",
         "The BCM is asleep. This is by design, not a fault.",
         "Run TC-C-01. Tap ignition BRIEFLY (under 0.8 s)."),
        ("Tapping ignition does nothing",
         "You are holding it too long, so it changes the lighting mode "
         "instead of toggling power.",
         "Press and release quickly, well under 0.8 s."),
        ("'unable to connect to the target'",
         "Board unpowered, SWDIO/SWCLK swapped, or no shared ground.",
         "Check the board's power LED FIRST. Then reseat the four SWD wires."),
        ("No LEDs light at all, ever",
         "Resistors too large, LEDs reversed, or the ground rail is not "
         "connected to a Blue Pill GND pin.",
         "Use 220 Ω–1 kΩ. Long LED leg toward the GPIO. Check the ground "
         "jumper."),
        ("One LED dark, the rest fine",
         "That LED is inserted backwards or its wire is open.",
         "Reverse the LED; reseat the jumper."),
        ("A button does nothing",
         "3.3 V is not reaching the GPIO when pressed, or its 10 kΩ does not "
         "land on ground.",
         "Check the GUI Inputs panel - if it does not light there either, "
         "it is wiring."),
        ("'no serial port found'",
         "The USB-UART adapter is not connected. The ST-Link does NOT carry "
         "this link.",
         "Plug in the separate USB-UART adapter."),
        ("'talk' times out",
         "RX/TX not crossed, or wired to PA9/PA10 instead of PB6/PB7.",
         "Adapter RX to PB6, adapter TX to PB7."),
        ("Diagnostic tool will not connect",
         "Another program holds the COM port.",
         "Close any window running 'talk'; only one may use the port."),
        ("Tool exits with 'You must install .NET'",
         "The SDK is installed per-user, so the app cannot find its runtime.",
         "Launch it with tools\\bcm.ps1 desktop, which sets DOTNET_ROOT."),
        ("Firmware hangs under QEMU",
         "Wrong QEMU. Version 9.x does not emulate the STM32F1 clock.",
         "Use the gnuarmeclipse 2.8 build."),
        ("The board's micro-USB port does nothing",
         "Normal. The STM32F103 has no USB bootloader in ROM.",
         "That port supplies power only. Flash via ST-Link."),
    ]
    for a, b, c in issues:
        r = tb.add_row()
        cell_text(r.cells[0], a)
        cell_text(r.cells[1], b)
        cell_text(r.cells[2], c)
        r.cells[0].width = Inches(2.3)
        r.cells[1].width = Inches(2.3)
        r.cells[2].width = Inches(2.2)

    doc.add_page_break()

    # ---- Part G ----------------------------------------------------------
    heading(doc, "Part G — Results summary", 1)
    para(doc, "Record the outcome of every test case.")

    res = doc.add_table(rows=1, cols=4)
    res.style = "Table Grid"
    for i, name in enumerate(["Test", "Description", "Result", "Notes"]):
        cell_text(res.cell(0, i), name, bold=True)
        shade(res.cell(0, i), "1F4E79")
        res.cell(0, i).paragraphs[0].runs[0].font.color.rgb = RGBColor(
            0xFF, 0xFF, 0xFF)

    summary = [
        ("TC-A-01", "Unit tests (189 cases)"),
        ("TC-A-02", "SIL scenarios (15)"),
        ("TC-A-03", "Protocol conformance (17)"),
        ("TC-A-04", "Emulated run"),
        ("TC-B-01", "Power and connections"),
        ("TC-B-02", "Programme the firmware"),
        ("TC-B-03", "CPU executing"),
        ("TC-B-04", "Protocol over the real link"),
        ("TC-C-01", "Wake the BCM"),
        ("TC-C-02", "Lighting mode cycle"),
        ("TC-C-03", "Indicators"),
        ("TC-C-04", "Horn and door locking"),
        ("TC-C-05", "Battery reading"),
        ("TC-D-01", "Brake lamp priority"),
        ("TC-D-02", "Hazard overrides indicators"),
        ("TC-D-03", "Load shedding protects safety lamps"),
        ("TC-D-04", "Sleep blanking"),
        ("TC-D-05", "Reverse gated on gear"),
        ("TC-E-01", "Tool launch and connect"),
        ("TC-E-02", "Live monitoring"),
        ("TC-E-03", "Driving outputs, reading faults"),
    ]
    for tc, desc in summary:
        r = res.add_row()
        cell_text(r.cells[0], tc, bold=True)
        cell_text(r.cells[1], desc)
        cell_text(r.cells[2], "☐ Pass    ☐ Fail")
        cell_text(r.cells[3], "")
        r.cells[0].width = Inches(0.8)
        r.cells[1].width = Inches(2.9)
        r.cells[2].width = Inches(1.5)
        r.cells[3].width = Inches(1.6)

    doc.add_paragraph()
    heading(doc, "Known limitations — not defects", 2)
    para(doc, "The following are documented limitations of version 1.0.0. Do "
              "not raise them as failures.")
    for text in [
        "Reverse-gear, door-ajar and light-mode switches, and the parking "
        "lamp, are not wired. Those inputs are inactive on hardware; they are "
        "covered by the SIL scenarios instead.",
        "No LDR is fitted, so auto-headlight reports full daylight on the "
        "board. It is verified in the SIL.",
        "Configuration is not stored in flash, so tunables reset on power "
        "cycle.",
        "The lighting mode is changed by holding the ignition button, because "
        "no dedicated light switch is wired.",
        "QEMU timing is approximate and must not be used to judge blink "
        "rates.",
    ]:
        bullet(doc, text)

    doc.add_paragraph()
    sign = doc.add_table(rows=3, cols=2)
    sign.style = "Table Grid"
    for i, (k, v) in enumerate([
        ("Overall result", "☐ PASS          ☐ FAIL"),
        ("Tester signature", ""),
        ("Date", ""),
    ]):
        cell_text(sign.cell(i, 0), k, bold=True)
        cell_text(sign.cell(i, 1), v)
        sign.columns[0].width = Inches(1.8)
        sign.columns[1].width = Inches(4.9)

    doc.add_paragraph()
    para(doc,
         "Reference documents: BCM-SRS-001 (requirements), BCM-ICD-001 "
         "(protocol), BCM-TST-001 (test report), BCM-PRJ-001 (project "
         "report), BCM-MAN-001 (user manual).",
         size=9, italic=True, colour=MUTED)

    OUT.parent.mkdir(parents=True, exist_ok=True)
    doc.save(OUT)
    return OUT


if __name__ == "__main__":
    path = build()
    print(f"written: {path}  ({path.stat().st_size} bytes)")
