#!/usr/bin/env python3
"""
Generate the BCM Simulator command reference as a Word document.

Kept as a script rather than a hand-edited .docx so the reference stays
reviewable in a diff and can be regenerated whenever a command changes.

    python tools/make_command_reference.py

Output: docs/CommandReference.docx
"""

from datetime import datetime
from pathlib import Path

from docx import Document
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor

ACCENT = RGBColor(0x1F, 0x4E, 0x79)
MUTED = RGBColor(0x59, 0x59, 0x59)
DANGER = RGBColor(0xC0, 0x00, 0x00)
OKGREEN = RGBColor(0x1E, 0x7A, 0x3C)

AUTHOR = "Akshansh Jha"
ISSUED = datetime(2026, 8, 4, 9, 15)

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "CommandReference.docx"


# --------------------------------------------------------------------------
# helpers
# --------------------------------------------------------------------------

def shade(cell, hex_colour):
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:val"), "clear")
    shd.set(qn("w:color"), "auto")
    shd.set(qn("w:fill"), hex_colour)
    tc_pr.append(shd)


def cell_text(cell, text, *, bold=False, size=9, mono=False, colour=None):
    cell.text = ""
    p = cell.paragraphs[0]
    p.paragraph_format.space_after = Pt(0)
    for i, line in enumerate(str(text).split("\n")):
        if i:
            p = cell.add_paragraph()
            p.paragraph_format.space_after = Pt(0)
        run = p.add_run(line)
        run.bold = bold
        run.font.size = Pt(size)
        run.font.name = "Consolas" if mono else "Calibri"
        if colour:
            run.font.color.rgb = colour


def header_row(table, names, widths=None):
    for i, name in enumerate(names):
        cell_text(table.cell(0, i), name, bold=True)
        shade(table.cell(0, i), "1F4E79")
        table.cell(0, i).paragraphs[0].runs[0].font.color.rgb = RGBColor(
            0xFF, 0xFF, 0xFF)
        if widths:
            table.columns[i].width = widths[i]


def para(doc, text="", *, size=10.5, bold=False, italic=False, colour=None,
         space_after=6, mono=False):
    p = doc.add_paragraph()
    run = p.add_run(text)
    run.bold = bold
    run.italic = italic
    run.font.size = Pt(size)
    run.font.name = "Consolas" if mono else "Calibri"
    if colour:
        run.font.color.rgb = colour
    p.paragraph_format.space_after = Pt(space_after)
    return p


def bullet(doc, text, *, size=10):
    p = doc.add_paragraph(style="List Bullet")
    run = p.add_run(text)
    run.font.size = Pt(size)
    p.paragraph_format.space_after = Pt(3)
    return p


def code_block(doc, lines):
    table = doc.add_table(rows=1, cols=1)
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


def callout(doc, title, body, colour):
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


def command_entry(doc, syntax, purpose, hardware, output_lines, notes=None):
    """One command: syntax, what it does, whether a board is needed."""
    p = doc.add_paragraph()
    r = p.add_run(syntax)
    r.font.name = "Consolas"
    r.font.size = Pt(11)
    r.bold = True
    r.font.color.rgb = ACCENT
    p.paragraph_format.space_after = Pt(2)

    t = doc.add_table(rows=2, cols=2)
    t.style = "Table Grid"
    t.columns[0].width = Inches(1.15)
    t.columns[1].width = Inches(5.55)
    cell_text(t.cell(0, 0), "Purpose", bold=True)
    cell_text(t.cell(0, 1), purpose)
    cell_text(t.cell(1, 0), "Hardware", bold=True)
    cell_text(t.cell(1, 1), hardware)
    doc.add_paragraph().paragraph_format.space_after = Pt(2)

    if output_lines:
        para(doc, "Typical output:", size=9, italic=True, colour=MUTED,
             space_after=2)
        code_block(doc, output_lines)

    if notes:
        for n in notes:
            bullet(doc, n, size=9.5)

    doc.add_paragraph().paragraph_format.space_after = Pt(8)


def set_properties(doc, title, doc_id):
    """Stamp the document properties Word shows under File > Info.

    The issue date is fixed rather than "now" so the properties do not drift
    every time the document is regenerated. The .docx bytes still differ
    between runs, because the zip container stores its own entry timestamps.
    """
    props = doc.core_properties
    props.author = AUTHOR
    props.last_modified_by = AUTHOR
    props.title = title
    props.subject = doc_id
    props.category = "Engineering documentation"
    props.comments = ""
    props.revision = 1
    props.created = ISSUED
    props.modified = ISSUED


# --------------------------------------------------------------------------
# document
# --------------------------------------------------------------------------

def build():
    doc = Document()
    set_properties(doc, "BCM Simulator - Command Reference", "BCM-CMD-001")
    doc.styles["Normal"].font.name = "Calibri"
    doc.styles["Normal"].font.size = Pt(10.5)

    for s in doc.sections:
        s.top_margin = Inches(0.7)
        s.bottom_margin = Inches(0.7)
        s.left_margin = Inches(0.8)
        s.right_margin = Inches(0.8)

    # ---- Cover -----------------------------------------------------------
    t = doc.add_heading("Command Reference", level=0)
    for run in t.runs:
        run.font.color.rgb = ACCENT
    para(doc, "Automotive Body Control Module (BCM) Simulator",
         size=15, bold=True, colour=MUTED, space_after=14)

    info = doc.add_table(rows=4, cols=2)
    info.style = "Table Grid"
    for i, (k, v) in enumerate([
        ("Document ID", "BCM-CMD-001"),
        ("Version", "1.0"),
        ("Software version", "1.0.0"),
        ("Repository", r"C:\Users\Akshansh\bcm-simulator"),
    ]):
        cell_text(info.cell(i, 0), k, bold=True)
        cell_text(info.cell(i, 1), v)
        info.columns[0].width = Inches(1.6)
        info.columns[1].width = Inches(5.1)

    doc.add_paragraph()
    callout(doc, "Two rules that apply to every command",
            "1. Run them from the repository root, not from a subfolder:\n"
            "       cd C:\\Users\\Akshansh\\bcm-simulator\n\n"
            "2. Use the long form for bcm.ps1 so PowerShell's execution "
            "policy does not block it:\n"
            "       powershell -ExecutionPolicy Bypass -File tools\\bcm.ps1 "
            "<action>\n\n"
            "Throughout this document that is shortened to "
            "'tools\\bcm.ps1 <action>' for readability.",
            OKGREEN)

    heading(doc, "Quick reference", 2)
    quick = doc.add_table(rows=1, cols=3)
    quick.style = "Table Grid"
    header_row(quick, ["Command", "What it does", "Board?"],
               [Inches(2.2), Inches(3.6), Inches(0.9)])
    for cmd, what, hw in [
        ("tools\\bcm.ps1 build", "Compile the firmware", "no"),
        ("tools\\bcm.ps1 test", "189 host unit tests", "no"),
        ("tools\\bcm.ps1 sil", "15 vehicle scenarios", "no"),
        ("tools\\bcm.ps1 run", "Run the firmware in QEMU", "no"),
        ("tools\\bcm.ps1 debug", "QEMU halted, waiting for GDB", "no"),
        ("tools\\bcm.ps1 docs", "Generate the API reference", "no"),
        ("tools\\bcm.ps1 clean", "Delete firmware build output", "no"),
        ("tools\\bcm.ps1 ports", "List serial ports", "no"),
        ("tools\\bcm.ps1 flash", "Programme the board", "YES"),
        ("tools\\bcm.ps1 verify", "Prove the CPU is executing", "YES"),
        ("tools\\bcm.ps1 talk", "Exercise the protocol over serial", "YES"),
        ("tools\\bcm.ps1 desktop", "Build and open the Windows GUI", "GUI"),
    ]:
        r = quick.add_row()
        cell_text(r.cells[0], cmd, mono=True)
        cell_text(r.cells[1], what)
        cell_text(r.cells[2], hw, bold=(hw == "YES"))

    doc.add_page_break()

    # ---- Section 1 -------------------------------------------------------
    heading(doc, "1. Build and clean", 1)

    command_entry(
        doc, "tools\\bcm.ps1 build",
        "Compiles the firmware for the STM32F103 and prints the image size. "
        "Fetches the vendor HAL and FreeRTOS automatically the first time.",
        "Not required.",
        ["arm-none-eabi-g++ ... -o build/bcm_firmware.elf",
         "   text    data     bss     dec     hex filename",
         "  24564     124    8456   33144    8178 build/bcm_firmware.elf"],
        ["text = flash used (38 % of 64 kB); bss = RAM used (43 % of 20 kB).",
         "Output lands in firmware/build/ as .elf, .hex and .bin."],
    )

    command_entry(
        doc, "tools\\bcm.ps1 clean",
        "Deletes the firmware build directory. Use it if a build behaves "
        "oddly after switching branches.",
        "Not required.",
        ["rm -rf build"],
    )

    doc.add_paragraph()
    heading(doc, "Equivalent make targets", 2)
    para(doc, "bcm.ps1 wraps these. Use them directly only if you have "
              "already put the toolchain on PATH yourself.", size=10,
         italic=True)

    mk = doc.add_table(rows=1, cols=3)
    mk.style = "Table Grid"
    header_row(mk, ["Directory", "Target", "Effect"],
               [Inches(1.2), Inches(1.6), Inches(3.9)])
    for d, target, effect in [
        ("firmware/", "make", "Build .elf, .hex, .bin and print the size"),
        ("firmware/", "make deps", "Clone STM32CubeF1 and FreeRTOS (one-off)"),
        ("firmware/", "make size", "Print the image size only"),
        ("firmware/", "make flash", "st-flash write (NOT used - bcm.ps1 uses OpenOCD)"),
        ("firmware/", "make monitor", "Print serial terminal hints"),
        ("firmware/", "make clean", "Remove firmware/build/"),
        ("tests/", "make", "Build the unit test binary"),
        ("tests/", "make run", "Build and run the unit tests"),
        ("tests/", "make deps", "Fetch the Catch2 single header"),
        ("tests/", "make clean", "Remove tests/build/"),
        ("sil/", "make", "Build the SIL harness"),
        ("sil/", "make run", "Build and run all scenarios"),
        ("sil/", "make list", "List scenario names"),
        ("sil/", "make clean", "Remove sil/build/"),
    ]:
        r = mk.add_row()
        cell_text(r.cells[0], d, mono=True)
        cell_text(r.cells[1], target, mono=True)
        cell_text(r.cells[2], effect)

    doc.add_page_break()

    # ---- Section 2 -------------------------------------------------------
    heading(doc, "2. Testing (no board needed)", 1)

    command_entry(
        doc, "tools\\bcm.ps1 test",
        "Builds and runs the host unit test suite - every module tested in "
        "isolation against hostile inputs.",
        "Not required. Runs natively on the PC.",
        ["All tests passed (3630 assertions in 189 test cases)",
         "PASS: all unit tests green."],
        ["Exits non-zero on failure, so it can gate a CI pipeline.",
         "Tests the same source files the firmware links, not a copy."],
    )

    command_entry(
        doc, "tools\\bcm.ps1 sil",
        "Runs the Software-in-the-Loop scenarios: the whole BCM driven "
        "through vehicle situations over simulated time.",
        "Not required.",
        ["  PASS  Brake lamp overrides everything     SRS-BRK-001, SAFETY-003",
         "  PASS  Doors never lock while one is open  SRS-DOOR-002, SAFETY-004",
         "scenarios: 15  passed: 15  failed: 0",
         "checks:    79  passed: 79  failed: 0",
         "RESULT: PASS"],
    )

    para(doc, "Variants:", bold=True, space_after=2)
    code_block(doc, [
        "tools\\bcm.ps1 sil -SilArgs --verbose          # list passing checks too",
        "tools\\bcm.ps1 sil -SilArgs --list             # print scenario names",
        "tools\\bcm.ps1 sil -SilArgs brake-priority     # run one scenario",
        "tools\\bcm.ps1 sil -SilArgs door-open-guard",
    ])

    command_entry(
        doc, "tools\\bcm.ps1 desktop -SilArgs test",
        "Runs the C# conformance suite: proves the Windows tool and the "
        "firmware agree on the protocol, byte for byte.",
        "Not required.",
        ["Passed! - Failed: 0, Passed: 17, Skipped: 0, Total: 17"],
    )

    doc.add_page_break()

    # ---- Section 3 -------------------------------------------------------
    heading(doc, "3. Running without hardware (QEMU)", 1)

    command_entry(
        doc, "tools\\bcm.ps1 run",
        "Runs the real firmware image in an STM32F103 emulator. The "
        "heartbeat LED is printed as text.",
        "Not required.",
        ["Board: 'BluePill' (BluePill STM32F103C8T6).",
         "[led:red off]",
         "[led:red on]",
         "[led:red off]"],
        ["Press Ctrl+C to stop.",
         "Add -Seconds 8 to stop automatically after 8 seconds.",
         "QEMU is NOT cycle-accurate: a 500 ms delay measures about 722 ms. "
         "Use it for logic, never for timing."],
    )

    command_entry(
        doc, "tools\\bcm.ps1 debug",
        "Starts QEMU with the processor halted, waiting for a debugger on "
        "port 1234. Press F5 in VS Code to attach.",
        "Not required.",
        ["==> QEMU halted at reset, waiting for GDB on localhost:1234"],
        ["Open the 'firmware' folder in VS Code, not the repository root.",
         "-Attach drops straight into a console GDB session instead.",
         "-GdbPort 4321 uses a different port."],
    )

    doc.add_page_break()

    # ---- Section 4 -------------------------------------------------------
    heading(doc, "4. Hardware", 1)

    command_entry(
        doc, "tools\\bcm.ps1 ports",
        "Lists the serial ports Windows can see. Use it to find your "
        "USB-UART adapter before connecting.",
        "The UART adapter must be plugged in.",
        ["Serial ports: COM13"],
        ["The ST-Link does NOT appear here - it is not a serial port."],
    )

    command_entry(
        doc, "tools\\bcm.ps1 flash",
        "Builds, then programmes the board over SWD using the ST-Link, and "
        "verifies what was written.",
        "ST-Link connected; the board's power LED must be lit.",
        ["Info : SWD DPIDR 0x2ba01477",
         "Info : [stm32f1x.cpu] Cortex-M3 r2p0 processor detected",
         "** Programming Finished **",
         "** Verified OK **",
         "==> Flash OK. PC13 should now blink at 1 Hz."],
        ["-Method swd forces SWD; -Method uart uses the ROM bootloader "
         "instead (needs BOOT0 = 1 and the door-lock LED unplugged from PA9).",
         "'unable to connect to the target' almost always means the board is "
         "unpowered or SWDIO/SWCLK are swapped."],
    )

    command_entry(
        doc, "tools\\bcm.ps1 verify",
        "Reads the processor's output register live over SWD and confirms "
        "the heartbeat pin is toggling. This proves the CPU is EXECUTING, "
        "not merely that bytes reached flash.",
        "ST-Link connected.",
        ["==> Sampling GPIOC->ODR every 250 ms for 3 s ...",
         "    PC13: *o**oo**o**o    ('*' = LED on, 'o' = LED off)",
         "PASS: PC13 is toggling (6 high / 6 low samples) - firmware is running."],
        ["'stuck HIGH/LOW' means the chip is powered but the program is not "
         "running - usually a crash or a failed clock configuration."],
    )

    command_entry(
        doc, "tools\\bcm.ps1 talk",
        "Speaks the diagnostic protocol to the board over the serial link "
        "and checks six commands, including two deliberately malformed ones.",
        "USB-UART adapter on PB6/PB7. Close the GUI first - only one program "
        "may hold the port.",
        ["  [ OK ] PING           echo verified",
         "  [ OK ] GET_VERSION    v1.0.0",
         "  [ OK ] GET_STATUS     lamps=0x0000 switches=0x00",
         "  [ OK ] GET_BATTERY    27.6% of full scale",
         "  [ OK ] bad command    answered UNKNOWN_CMD",
         "  [ OK ] short SETLAMP  answered BAD_LENGTH",
         "",
         "PASS: 6/6 protocol checks OK."],
        ["-Port COM13 selects a port explicitly if several are present.",
         "-Baud 115200 is the default and rarely needs changing."],
    )

    doc.add_page_break()

    # ---- Section 5 -------------------------------------------------------
    heading(doc, "5. The Windows diagnostic tool", 1)

    command_entry(
        doc, "tools\\bcm.ps1 desktop",
        "Builds and launches the graphical diagnostic tool. Select the COM "
        "port and press Connect.",
        "Board and UART adapter for live data; the window opens either way.",
        ["==> Building the diagnostic tool",
         "Build succeeded.",
         "==> Launching. Pick the COM port and press Connect."],
    )

    callout(doc, "Do not double-click the .exe",
            "desktop\\BcmDiagnosticTool\\bin\\Release\\net8.0-windows\\"
            "BcmDiagnosticTool.exe fails with 'You must install .NET to run "
            "this application'. The SDK here is installed per-user, so the "
            "program cannot locate its runtime by itself. Launch it through "
            "bcm.ps1, which sets DOTNET_ROOT - or set that variable yourself:"
            "\n\n"
            "    $env:DOTNET_ROOT = \"$env:USERPROFILE\\.dotnet\"\n"
            "    & .\\desktop\\BcmDiagnosticTool\\bin\\Release\\"
            "net8.0-windows\\BcmDiagnosticTool.exe --connect COM13",
            DANGER)

    para(doc, "Direct .NET commands:", bold=True, space_after=2)
    code_block(doc, [
        "$env:DOTNET_ROOT = \"$env:USERPROFILE\\.dotnet\"",
        "$env:PATH = \"$env:DOTNET_ROOT;$env:PATH\"",
        "",
        "dotnet build desktop\\BcmDiagnosticTool.sln -c Release",
        "dotnet test  desktop\\BcmDiagnosticTool.sln -c Release",
        "dotnet clean desktop\\BcmDiagnosticTool.sln",
    ])

    doc.add_page_break()

    # ---- Section 6 -------------------------------------------------------
    heading(doc, "6. Documentation", 1)

    command_entry(
        doc, "tools\\bcm.ps1 docs",
        "Generates the API reference from the source comments into "
        "docs/api/html/index.html.",
        "Not required.",
        ["==> Generating the API reference",
         "    no warnings",
         "==> Done: ...\\docs\\api\\html\\index.html"],
        ["-SilArgs open also opens it in your browser.",
         "docs/api/ is gitignored - it is generated output."],
    )

    para(doc, "Regenerating the Word documents:", bold=True, space_after=2)
    code_block(doc, [
        "python tools\\make_test_procedure.py     # -> docs/test/BCM-TestProcedure.docx",
        "python tools\\make_command_reference.py  # -> docs/CommandReference.docx",
    ])

    doc.add_paragraph()
    heading(doc, "Where the written documents live", 2)
    dl = doc.add_table(rows=1, cols=3)
    dl.style = "Table Grid"
    header_row(dl, ["Document", "ID", "Path"],
               [Inches(2.1), Inches(1.2), Inches(3.4)])
    for name, did, path in [
        ("Project Report", "BCM-PRJ-001", "docs/ProjectReport.md"),
        ("Requirements", "BCM-SRS-001", "docs/requirements/SRS.md"),
        ("Architecture", "BCM-ARC-001", "docs/architecture/ArchitectureDocument.md"),
        ("Protocol (ICD)", "BCM-ICD-001", "docs/protocol/ICD.md"),
        ("UML model", "BCM-UML-001", "docs/uml/UML.md"),
        ("Test report", "BCM-TST-001", "docs/test/TestReport.md"),
        ("Test procedure (Word)", "BCM-ATP-001", "docs/test/BCM-TestProcedure.docx"),
        ("User manual", "BCM-MAN-001", "docs/manual/UserManual.md"),
        ("Command reference (Word)", "BCM-CMD-001", "docs/CommandReference.docx"),
        ("Release notes", "-", "docs/ReleaseNotes.md"),
    ]:
        r = dl.add_row()
        cell_text(r.cells[0], name)
        cell_text(r.cells[1], did, mono=True)
        cell_text(r.cells[2], path, mono=True)

    doc.add_page_break()

    # ---- Section 7 -------------------------------------------------------
    heading(doc, "7. All bcm.ps1 parameters", 1)
    para(doc, "The action is positional and comes first. Everything else is "
              "optional and only applies to certain actions.")

    pt = doc.add_table(rows=1, cols=4)
    pt.style = "Table Grid"
    header_row(pt, ["Parameter", "Values", "Default", "Applies to"],
               [Inches(1.3), Inches(2.0), Inches(1.1), Inches(2.3)])
    for name, values, default, applies in [
        ("(action)", "build, run, debug, clean, flash,\nports, verify, test, "
                     "talk, sil,\ndesktop, docs", "run", "-"),
        ("-Seconds", "whole number", "0 (forever)", "run"),
        ("-GdbPort", "TCP port", "1234", "debug"),
        ("-Attach", "switch", "off", "debug"),
        ("-Port", "COM13 etc.", "auto-detect", "flash, talk"),
        ("-Baud", "whole number", "115200", "flash (uart), talk"),
        ("-Method", "auto, swd, uart", "auto", "flash"),
        ("-SilArgs", "list of strings", "empty", "sil, desktop, docs"),
    ]:
        r = pt.add_row()
        cell_text(r.cells[0], name, mono=True, bold=True)
        cell_text(r.cells[1], values, mono=True)
        cell_text(r.cells[2], default, mono=True)
        cell_text(r.cells[3], applies)

    doc.add_paragraph()
    heading(doc, "Examples", 2)
    code_block(doc, [
        "tools\\bcm.ps1 run -Seconds 8              # emulate for 8 s then stop",
        "tools\\bcm.ps1 debug -Attach               # QEMU + console GDB",
        "tools\\bcm.ps1 flash -Method uart -Port COM13",
        "tools\\bcm.ps1 talk -Port COM13",
        "tools\\bcm.ps1 sil -SilArgs --verbose",
        "tools\\bcm.ps1 docs -SilArgs open",
        "tools\\bcm.ps1 desktop -SilArgs test",
    ])

    doc.add_page_break()

    # ---- Section 8 -------------------------------------------------------
    heading(doc, "8. One-time setup commands", 1)
    para(doc, "Everything installs per-user. No administrator rights are "
              "needed and nothing is added to PATH - bcm.ps1 locates each "
              "tool itself.")

    code_block(doc, [
        "# Embedded toolchain, build tools, debugger",
        "xpm install --global @xpack-dev-tools/arm-none-eabi-gcc@latest",
        "xpm install --global @xpack-dev-tools/windows-build-tools@latest",
        "xpm install --global @xpack-dev-tools/openocd@latest",
        "",
        "# Host compiler, for the unit tests and the SIL",
        "xpm install --global @xpack-dev-tools/mingw-w64-gcc@latest",
        "",
        "# Emulator - MUST be the 2.8 gnuarmeclipse build, not 9.x",
        "xpm install --global @xpack-dev-tools/qemu-arm@2.8.0-13.1",
        "",
        "# .NET, for the Windows diagnostic tool",
        "$s = \"$env:TEMP\\dotnet-install.ps1\"",
        "Invoke-WebRequest https://dot.net/v1/dotnet-install.ps1 -OutFile $s "
        "-UseBasicParsing",
        "& $s -Channel 8.0 -InstallDir \"$env:USERPROFILE\\.dotnet\" -NoPath",
        "",
        "# Doxygen - portable, just unzip it",
        "Invoke-WebRequest "
        "https://www.doxygen.nl/files/doxygen-1.12.0.windows.x64.bin.zip `",
        "    -OutFile \"$env:TEMP\\doxygen.zip\" -UseBasicParsing",
        "Expand-Archive \"$env:TEMP\\doxygen.zip\" "
        "\"$env:USERPROFILE\\.local\\doxygen\"",
        "",
        "# Python helper used to generate the Word documents",
        "python -m pip install --user python-docx",
        "",
        "# Fetch the vendor HAL and FreeRTOS (not stored in the repository)",
        "cd firmware; make deps",
    ])

    callout(doc, "QEMU version matters",
            "Upstream QEMU 9.x maps the STM32F1 clock peripheral as a stub "
            "that always reads zero, so the firmware hangs forever waiting "
            "for the oscillator and gives no clue why. Only the "
            "gnuarmeclipse 2.8 build emulates this chip properly.",
            DANGER)

    doc.add_page_break()

    # ---- Section 9 -------------------------------------------------------
    heading(doc, "9. Advanced: OpenOCD and GDB directly", 1)
    para(doc, "bcm.ps1 wraps these. Use them when you want on-chip debugging "
              "or to inspect a register by hand.", size=10, italic=True)

    para(doc, "Start an OpenOCD session (leaves a GDB server on port 3333):",
         bold=True, space_after=2)
    code_block(doc, [
        "$R = \"$env:APPDATA\\xPacks\\@xpack-dev-tools\\openocd\\"
        "0.12.0-7.1\\.content\"",
        "& \"$R\\bin\\openocd.exe\" -s \"$R\\openocd\\scripts\" `",
        "    -f interface/stlink.cfg -c \"transport select swd\" `",
        "    -f target/stm32f1x.cfg",
    ])

    para(doc, "Attach GDB to a running target:", bold=True, space_after=2)
    code_block(doc, [
        "$G = \"$env:APPDATA\\xPacks\\@xpack-dev-tools\\arm-none-eabi-gcc\\"
        "15.2.1-1.1.1\\.content\\bin\"",
        "cd firmware",
        "& \"$G\\arm-none-eabi-gdb.exe\" build\\bcm_firmware.elf",
        "",
        "(gdb) target extended-remote localhost:3333",
        "(gdb) interrupt            # stop the processor",
        "(gdb) backtrace            # where is it?",
        "(gdb) break bsp.cpp:37",
        "(gdb) continue",
        "(gdb) info registers pc",
    ])

    para(doc, "Read a peripheral register without stopping the program:",
         bold=True, space_after=2)
    code_block(doc, [
        "# inside an OpenOCD session, or via -c on the command line",
        "mdw 0x4001100c      # GPIOC output register (bit 13 = heartbeat LED)",
        "mdw 0x40010c0c      # GPIOB output register (8 of the lamps)",
        "mdw 0x40010808      # GPIOA input register  (bits 0-5 = the buttons)",
    ])
    para(doc, "These are how the sleep-state diagnosis in the test procedure "
              "was made: reading the lamp outputs directly showed only the "
              "door-lock lamp lit, which is the signature of a sleeping BCM.",
         size=10, italic=True, colour=MUTED)

    doc.add_paragraph()
    heading(doc, "Git commands used in this project", 2)
    code_block(doc, [
        "git status --short           # what has changed",
        "git log --oneline            # commit history",
        "git diff                     # unstaged changes",
        "git add -A                   # stage everything",
        "git commit                   # record a commit",
        "git checkout -b <branch>     # start a branch",
        "git merge --ff-only <branch> # fast-forward merge",
    ])

    doc.add_page_break()

    # ---- Section 10 ------------------------------------------------------
    heading(doc, "10. Recipes", 1)

    heading(doc, "Full regression, in order", 2)
    code_block(doc, [
        "tools\\bcm.ps1 test                    # 1. unit tests",
        "tools\\bcm.ps1 sil                     # 2. scenarios",
        "tools\\bcm.ps1 desktop -SilArgs test   # 3. protocol conformance",
        "tools\\bcm.ps1 build                   # 4. firmware compiles",
        "tools\\bcm.ps1 flash                   # 5. programme the board",
        "tools\\bcm.ps1 verify                  # 6. CPU is running",
        "tools\\bcm.ps1 talk                    # 7. protocol on hardware",
        "tools\\bcm.ps1 docs                    # 8. documentation builds",
    ])

    heading(doc, "I changed the firmware - what do I run?", 2)
    code_block(doc, [
        "tools\\bcm.ps1 test      # did I break a module?",
        "tools\\bcm.ps1 sil       # did I break vehicle behaviour?",
        "tools\\bcm.ps1 flash     # put it on the board",
        "tools\\bcm.ps1 verify    # is it actually running?",
    ])

    heading(doc, "I have no board today", 2)
    code_block(doc, [
        "tools\\bcm.ps1 test",
        "tools\\bcm.ps1 sil",
        "tools\\bcm.ps1 run",
        "tools\\bcm.ps1 desktop -SilArgs test",
    ])

    heading(doc, "Something is wrong with the board", 2)
    code_block(doc, [
        "tools\\bcm.ps1 ports     # is the UART adapter there?",
        "tools\\bcm.ps1 verify    # is the CPU executing?",
        "tools\\bcm.ps1 talk      # does it answer the protocol?",
        "tools\\bcm.ps1 flash     # reprogramme it",
    ])

    doc.add_paragraph()
    callout(doc, "If the lamps do not respond to the buttons",
            "That is usually not a fault. The BCM starts in SLEEP, where "
            "every lamp except the door-lock indicator is deliberately kept "
            "off. Tap the ignition button BRIEFLY - under 0.8 seconds - to "
            "wake it. Holding the button longer changes the lighting mode "
            "instead and does not wake the vehicle. See BCM-ATP-001, "
            "test case TC-C-01.",
            DANGER)

    doc.add_paragraph()
    para(doc, "Related documents: BCM-ATP-001 (test procedure), BCM-PRJ-001 "
              "(project report), BCM-MAN-001 (user manual), BCM-ICD-001 "
              "(protocol).", size=9, italic=True, colour=MUTED)

    OUT.parent.mkdir(parents=True, exist_ok=True)
    doc.save(OUT)
    return OUT


if __name__ == "__main__":
    path = build()
    print(f"written: {path}  ({path.stat().st_size} bytes)")
