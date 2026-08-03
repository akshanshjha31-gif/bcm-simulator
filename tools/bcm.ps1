<#
.SYNOPSIS
    BCM Simulator - build / run / debug helper.

.DESCRIPTION
    Locates the xPack toolchain (arm-none-eabi-gcc, GNU make) and the
    gnuarmeclipse QEMU automatically, so none of them need to be on PATH.

    Usage (from anywhere):
        powershell -ExecutionPolicy Bypass -File tools\bcm.ps1 build
        powershell -ExecutionPolicy Bypass -File tools\bcm.ps1 run
        powershell -ExecutionPolicy Bypass -File tools\bcm.ps1 debug
        powershell -ExecutionPolicy Bypass -File tools\bcm.ps1 clean

    run    - builds, then runs the firmware in QEMU. The PC13 heartbeat is
             printed as [led:red on] / [led:red off]. Ctrl+C to stop, or pass
             -Seconds N to stop automatically.
    debug  - builds, then starts QEMU with the core HALTED, waiting for GDB on
             port 1234. Press F5 in VS Code, or use -Attach to drop straight
             into a console GDB session.

    NOTE: QEMU must be the *gnuarmeclipse* 2.8 build. Upstream QEMU 9.x has no
    working STM32F1 RCC model, so the firmware hangs polling HSERDY forever.
    Install with:  xpm install --global @xpack-dev-tools/qemu-arm@2.8.0-13.1
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('build', 'run', 'debug', 'clean', 'flash', 'ports', 'verify', 'test', 'talk')]
    [string]$Task = 'run',

    [int]$GdbPort = 1234,
    [int]$Seconds = 0,
    [string]$Port,
    [int]$Baud = 115200,
    [ValidateSet('auto', 'swd', 'uart')]
    [string]$Method = 'auto',
    [switch]$Attach
)

$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Firmware = Join-Path $RepoRoot 'firmware'
$Elf      = 'build/bcm_firmware.elf'
$XPackDir = Join-Path $env:APPDATA 'xPacks\@xpack-dev-tools'

# Newest version of an xPack that actually contains the requested executable.
function Find-XPackExe {
    param([string]$Package, [string]$Exe)
    $root = Join-Path $XPackDir $Package
    if (-not (Test-Path $root)) { return $null }
    Get-ChildItem $root -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        ForEach-Object {
            $candidate = Join-Path $_.FullName ".content\bin\$Exe"
            if (Test-Path $candidate) { $candidate }
        } | Select-Object -First 1
}

# OpenOCD ships its config scripts alongside the binary; return both.
function Find-OpenOcd {
    $root = Get-ChildItem (Join-Path $XPackDir 'openocd') -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending |
            ForEach-Object { if (Test-Path (Join-Path $_.FullName '.content\bin\openocd.exe')) { $_.FullName } } |
            Select-Object -First 1
    if (-not $root) { return $null }
    [pscustomobject]@{
        Exe     = Join-Path $root '.content\bin\openocd.exe'
        Scripts = Join-Path $root '.content\openocd\scripts'
    }
}

function Require($Path, $What, $Hint) {
    if (-not $Path) { Write-Host "ERROR: $What not found.`n       $Hint" -ForegroundColor Red; exit 1 }
    return $Path
}

$gcc  = Require (Find-XPackExe 'arm-none-eabi-gcc' 'arm-none-eabi-gcc.exe') `
        'arm-none-eabi-gcc' 'xpm install --global @xpack-dev-tools/arm-none-eabi-gcc@latest'
$make = Require (Find-XPackExe 'windows-build-tools' 'make.exe') `
        'GNU make' 'xpm install --global @xpack-dev-tools/windows-build-tools@latest'

# Put the toolchain on PATH for the child make/gcc processes.
$env:PATH = "$(Split-Path -Parent $gcc);$(Split-Path -Parent $make);$env:PATH"

# --- Host unit tests (run natively, not on the STM32) ----------------------
if ($Task -eq 'test') {
    $hostCxx = Find-XPackExe 'mingw-w64-gcc' 'x86_64-w64-mingw32-g++.exe'
    if (-not $hostCxx) {
        Write-Host 'ERROR: host compiler not found.' -ForegroundColor Red
        Write-Host '       xpm install --global @xpack-dev-tools/mingw-w64-gcc@latest' -ForegroundColor Yellow
        exit 1
    }
    $env:PATH = "$(Split-Path -Parent $hostCxx);$env:PATH"

    $testDir = Join-Path $RepoRoot 'tests'
    Push-Location $testDir
    try {
        if (-not (Test-Path 'vendor/catch2/catch.hpp')) {
            Write-Host '==> Fetching Catch2' -ForegroundColor Cyan
            & $make deps
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }

        Write-Host '==> Building host unit tests' -ForegroundColor Cyan
        & $make
        if ($LASTEXITCODE -ne 0) { Write-Host 'Test build failed.' -ForegroundColor Red; exit $LASTEXITCODE }

        Write-Host '==> Running' -ForegroundColor Cyan
        $exe  = Join-Path $testDir 'build\bcm_tests.exe'
        $outF = Join-Path $env:TEMP "bcm_tests_$PID.out"
        $errF = Join-Path $env:TEMP "bcm_tests_$PID.err"
        # -PassThru is essential: Start-Process does NOT set $LASTEXITCODE, so
        # without it a failing suite would still be reported as a pass.
        $proc = Start-Process -FilePath $exe -Wait -NoNewWindow -PassThru `
                              -RedirectStandardOutput $outF -RedirectStandardError $errF
        $rc = $proc.ExitCode
        Get-Content $outF -Raw -ErrorAction SilentlyContinue
        Get-Content $errF -Raw -ErrorAction SilentlyContinue
        Remove-Item $outF, $errF -Force -ErrorAction SilentlyContinue

        if ($rc -eq 0) { Write-Host 'PASS: all unit tests green.' -ForegroundColor Green }
        else { Write-Host "FAIL: unit tests reported failures (exit $rc)." -ForegroundColor Red }
        exit $rc
    }
    finally { Pop-Location }
}

Push-Location $Firmware
try {
    if ($Task -eq 'clean') { & $make clean; exit $LASTEXITCODE }

    # Read GPIOC->ODR live over SWD and prove bit 13 (PC13) is actually
    # toggling. This checks the chip is executing, not just that flash verified.
    if ($Task -eq 'verify') {
        $ocd = Find-OpenOcd
        if (-not $ocd) { Write-Host 'ERROR: OpenOCD not found.' -ForegroundColor Red; exit 1 }

        $GPIOC_ODR = '0x4001100c'
        $samples   = 12
        $periodMs  = 250

        Write-Host "==> Sampling GPIOC->ODR every $periodMs ms for $($samples * $periodMs / 1000) s ..." -ForegroundColor Cyan
        $a = @('-s', $ocd.Scripts, '-f', 'interface/stlink.cfg', '-c', 'transport select swd',
               '-f', 'target/stm32f1x.cfg', '-c', 'init')
        for ($i = 0; $i -lt $samples; $i++) { $a += @('-c', "mdw $GPIOC_ODR", '-c', "sleep $periodMs") }
        $a += @('-c', 'shutdown')

        # NOTE: do not pipe `2>&1` here - in Windows PowerShell 5.1 that wraps
        # each native stderr line in an ErrorRecord and trips $ErrorActionPreference.
        $outFile = Join-Path $env:TEMP "bcm_verify_$PID.out"
        $errFile = Join-Path $env:TEMP "bcm_verify_$PID.err"
        # Start-Process joins ArgumentList with spaces and does NOT quote, so
        # multi-word args ("transport select swd") must be quoted by hand.
        $quoted = $a | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }
        Start-Process -FilePath $ocd.Exe -ArgumentList $quoted -Wait -NoNewWindow `
                      -RedirectStandardOutput $outFile -RedirectStandardError $errFile
        $out = (Get-Content $outFile -Raw -ErrorAction SilentlyContinue) +
               (Get-Content $errFile -Raw -ErrorAction SilentlyContinue)
        Remove-Item $outFile, $errFile -Force -ErrorAction SilentlyContinue

        if ($out -match 'unable to connect to the target') {
            Write-Host 'FAIL: cannot reach the chip. Check ST-Link wiring and that the power LED is lit.' -ForegroundColor Red
            exit 1
        }

        $bits = [regex]::Matches($out, '(?im)^0x4001100c:\s*([0-9a-f]+)') |
                ForEach-Object { ([Convert]::ToUInt32($_.Groups[1].Value, 16) -shr 13) -band 1 }

        if ($bits.Count -lt 2) { Write-Host 'FAIL: no register reads returned.' -ForegroundColor Red; exit 1 }

        $pattern = ($bits | ForEach-Object { if ($_ -eq 1) { 'o' } else { '*' } }) -join ''
        Write-Host "    PC13: $pattern    ('*' = LED on, 'o' = LED off)"

        $high = ($bits | Where-Object { $_ -eq 1 }).Count
        $low  = ($bits | Where-Object { $_ -eq 0 }).Count
        if ($high -gt 0 -and $low -gt 0) {
            Write-Host "PASS: PC13 is toggling ($high high / $low low samples) - firmware is running." -ForegroundColor Green
            exit 0
        }
        $stuck = if ($high -gt 0) { 'HIGH (LED off)' } else { 'LOW (LED on)' }
        Write-Host "FAIL: PC13 is stuck $stuck - the chip is powered but not running the loop." -ForegroundColor Red
        exit 1
    }

    # Speak BCM-ICD-001 to the board over the diagnostic UART and check the
    # replies. This is an independent implementation of the protocol - if it
    # agrees with the firmware, the ICD is genuinely unambiguous.
    if ($Task -eq 'talk') {
        function Get-Crc8 {
            param([byte[]]$Data)
            $crc = 0
            foreach ($b in $Data) {
                $crc = $crc -bxor $b
                for ($i = 0; $i -lt 8; $i++) {
                    if ($crc -band 0x80) { $crc = (($crc -shl 1) -bxor 0x07) -band 0xFF }
                    else                 { $crc = ($crc -shl 1) -band 0xFF }
                }
            }
            return [byte]$crc
        }

        function New-BcmFrame {
            param([byte]$Cmd, [byte[]]$Payload = @())
            $body = @($Cmd, [byte]$Payload.Count) + $Payload
            return ,([byte[]](@(0xAA) + $body + @((Get-Crc8 $body), 0x55)))
        }

        # Mirror of the firmware's FrameParser, so a disagreement shows up here.
        function Read-BcmFrame {
            param($Sp, [int]$TimeoutMs = 1000)
            $state = 'HDR'; $cmd = 0; $len = 0; $payload = @(); $body = @()
            $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
            while ((Get-Date) -lt $deadline) {
                if ($Sp.BytesToRead -le 0) { Start-Sleep -Milliseconds 5; continue }
                $b = $Sp.ReadByte()
                switch ($state) {
                    'HDR'  { if ($b -eq 0xAA) { $state = 'CMD'; $body = @() } }
                    'CMD'  { $cmd = $b; $body += $b; $state = 'LEN' }
                    'LEN'  {
                        $len = $b; $body += $b; $payload = @()
                        $state = if ($len -eq 0) { 'CKSUM' } else { 'PAYLOAD' }
                    }
                    'PAYLOAD' {
                        $payload += $b; $body += $b
                        if ($payload.Count -ge $len) { $state = 'CKSUM' }
                    }
                    'CKSUM' {
                        if ($b -ne (Get-Crc8 $body)) { return @{ Ok=$false; Why='checksum mismatch' } }
                        $state = 'FTR'
                    }
                    'FTR' {
                        if ($b -ne 0x55) { return @{ Ok=$false; Why='bad footer' } }
                        return @{ Ok=$true; Cmd=$cmd; Len=$len; Payload=$payload }
                    }
                }
            }
            return @{ Ok=$false; Why='timeout - no reply' }
        }

        if (-not $Port) {
            $found = @([System.IO.Ports.SerialPort]::GetPortNames())
            if ($found.Count -eq 1) { $Port = $found[0] }
            elseif ($found.Count -gt 1) {
                Write-Host "Multiple ports: $($found -join ', '). Re-run with -Port COMx" -ForegroundColor Yellow; exit 1
            }
            else {
                Write-Host @'
ERROR: no serial port found.

The diagnostic protocol needs a USB-UART adapter on USART1, which is
REMAPPED to PB6/PB7 on this board (PA9 drives the door-lock lamp):

    adapter RX  <---- PB6    (BCM TX)
    adapter TX  ----> PB7    (BCM RX)
    adapter GND <---> GND
    (3.3 V logic, 115200 8N1)

The ST-Link does NOT carry this - it is a separate adapter.
'@ -ForegroundColor Red
                exit 1
            }
        }

        Write-Host "==> Talking BCM-ICD-001 on $Port @ 115200 8N1" -ForegroundColor Cyan
        $sp = New-Object System.IO.Ports.SerialPort $Port, 115200, 'None', 8, 'One'
        $pass = 0; $fail = 0
        try {
            $sp.ReadTimeout = 1000; $sp.Open(); Start-Sleep -Milliseconds 200
            $sp.DiscardInBuffer()

            function Invoke-Check {
                param([string]$Name, [byte]$Cmd, [byte[]]$Payload = @(), [scriptblock]$Validate)
                $frame = New-BcmFrame -Cmd $Cmd -Payload $Payload
                $script:sp.Write($frame, 0, $frame.Length)
                $r = Read-BcmFrame -Sp $script:sp
                if (-not $r.Ok) {
                    Write-Host ("  [FAIL] {0,-14} {1}" -f $Name, $r.Why) -ForegroundColor Red
                    return $false
                }
                $expected = $Cmd -bor 0x80
                if ($r.Cmd -ne $expected) {
                    Write-Host ("  [FAIL] {0,-14} expected cmd 0x{1:X2}, got 0x{2:X2}" -f $Name, $expected, $r.Cmd) -ForegroundColor Red
                    return $false
                }
                $detail = & $Validate $r
                Write-Host ("  [ OK ] {0,-14} {1}" -f $Name, $detail) -ForegroundColor Green
                return $true
            }

            $checks = @(
                @{ N='PING';        C=0x01; P=[byte[]]@(0xDE,0xAD); V={ param($r)
                        if ($r.Payload[1] -ne 0xDE -or $r.Payload[2] -ne 0xAD) { throw 'payload not echoed' }
                        'echo verified' } },
                @{ N='GET_VERSION'; C=0x02; P=[byte[]]@();          V={ param($r)
                        "v$($r.Payload[1]).$($r.Payload[2]).$($r.Payload[3])" } },
                @{ N='GET_STATUS';  C=0x03; P=[byte[]]@();          V={ param($r)
                        $lamps = ($r.Payload[1] -shl 8) -bor $r.Payload[2]
                        "lamps=0x{0:X4} switches=0x{1:X2}" -f $lamps, $r.Payload[3] } },
                @{ N='GET_BATTERY'; C=0x08; P=[byte[]]@();          V={ param($r)
                        $pm = ($r.Payload[1] -shl 8) -bor $r.Payload[2]
                        "{0:N1}% of full scale" -f ($pm / 10.0) } },
                @{ N='bad command'; C=0x7E; P=[byte[]]@();          V={ param($r)
                        if ($r.Payload[0] -ne 0x01) { throw "expected UNKNOWN_CMD, got 0x$('{0:X2}' -f $r.Payload[0])" }
                        'answered UNKNOWN_CMD' } },
                @{ N='short SETLAMP'; C=0x04; P=[byte[]]@(0x07);    V={ param($r)
                        if ($r.Payload[0] -ne 0x02) { throw "expected BAD_LENGTH, got 0x$('{0:X2}' -f $r.Payload[0])" }
                        'answered BAD_LENGTH' } }
            )

            foreach ($c in $checks) {
                try {
                    if (Invoke-Check -Name $c.N -Cmd $c.C -Payload $c.P -Validate $c.V) { $pass++ } else { $fail++ }
                }
                catch {
                    Write-Host ("  [FAIL] {0,-14} {1}" -f $c.N, $_.Exception.Message) -ForegroundColor Red
                    $fail++
                }
            }
        }
        catch {
            Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
            exit 1
        }
        finally { if ($sp.IsOpen) { $sp.Close() } }

        Write-Host ""
        if ($fail -eq 0) { Write-Host "PASS: $pass/$($pass+$fail) protocol checks OK." -ForegroundColor Green; exit 0 }
        Write-Host "FAIL: $fail of $($pass+$fail) protocol checks failed." -ForegroundColor Red
        exit 1
    }

    if ($Task -eq 'ports') {
        $found = [System.IO.Ports.SerialPort]::GetPortNames()
        if ($found) { Write-Host "Serial ports: $($found -join ', ')" -ForegroundColor Green }
        else { Write-Host 'No serial ports. Plug in the USB-UART adapter (the board''s own micro-USB will not appear).' -ForegroundColor Yellow }
        exit 0
    }

    if (-not (Test-Path 'vendor/STM32CubeF1')) {
        Write-Host '==> Vendor HAL missing, running: make deps' -ForegroundColor Cyan
        & $make deps
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    Write-Host '==> Building' -ForegroundColor Cyan
    & $make
    if ($LASTEXITCODE -ne 0) { Write-Host 'Build failed.' -ForegroundColor Red; exit $LASTEXITCODE }
    if ($Task -eq 'build') { exit 0 }

    if ($Task -eq 'flash') {
        # Prefer SWD when an ST-Link is plugged in: faster, verified, and it
        # does not need the BOOT0 jumper dance.
        $stlink = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
                  Where-Object { $_.InstanceId -match 'VID_0483&PID_374[0-9B-F]' }
        if ($Method -eq 'auto') { $Method = if ($stlink) { 'swd' } else { 'uart' } }

        if ($Method -eq 'swd') {
            $ocdRoot = Get-ChildItem (Join-Path $XPackDir 'openocd') -Directory -ErrorAction SilentlyContinue |
                       Sort-Object Name -Descending |
                       ForEach-Object { if (Test-Path (Join-Path $_.FullName '.content\bin\openocd.exe')) { $_.FullName } } |
                       Select-Object -First 1
            if (-not $ocdRoot) {
                Write-Host 'ERROR: OpenOCD not found. xpm install --global @xpack-dev-tools/openocd@latest' -ForegroundColor Red; exit 1
            }
            $ocd     = Join-Path $ocdRoot '.content\bin\openocd.exe'
            $scripts = Join-Path $ocdRoot '.content\openocd\scripts'
            if (-not $stlink) { Write-Host 'WARNING: no ST-Link detected, trying anyway.' -ForegroundColor Yellow }

            Write-Host '==> Flashing over SWD (ST-Link V2)' -ForegroundColor Cyan
            & $ocd -s $scripts -f interface/stlink.cfg -c 'transport select swd' `
                   -f target/stm32f1x.cfg `
                   -c "program $Elf verify reset exit"
            $rc = $LASTEXITCODE
            if ($rc -eq 0) { Write-Host '==> Flash OK. PC13 should now blink at 1 Hz.' -ForegroundColor Green }
            else {
                Write-Host @"
==> Flash FAILED (exit $rc).

If the error was "unable to connect to the target", the SWD bus is silent.
Check, in this order:
  1. SWDIO -> board pin marked DIO/SWDIO (PA13), SWCLK -> CLK/SWCLK (PA14).
     Swapping these two is the single most common cause.
  2. GND connected between ST-Link and board. Without it SWD cannot work.
  3. Board's power LED is lit. If not, the micro-USB cable may be charge-only.
  4. Use the ST-Link's 3.3V pin - never the 5V pin - if you power from it.
  5. Reseat the dupont wires; cheap jumper leads fail open very often.
"@ -ForegroundColor Red
            }
            exit $rc
        }

        # --- UART ROM bootloader fallback (USART1 PA9/PA10) -----------------
        $py = (Get-Command python -ErrorAction SilentlyContinue).Source
        if (-not $py) { Write-Host 'ERROR: python not found (stm32loader needs it).' -ForegroundColor Red; exit 1 }

        if (-not $Port) {
            $found = @([System.IO.Ports.SerialPort]::GetPortNames())
            if ($found.Count -eq 1) { $Port = $found[0] }
            elseif ($found.Count -gt 1) {
                Write-Host "Multiple ports found: $($found -join ', '). Re-run with  -Port COMx" -ForegroundColor Yellow; exit 1
            }
            else {
                Write-Host @'
ERROR: no serial port found.

No ST-Link was detected either, so SWD is not available.

The Blue Pill's own micro-USB CANNOT flash a blank chip - the STM32F103 ROM
bootloader is UART-only (there is no USB DFU in ROM). The ROM bootloader
lives on the FACTORY pins PA9/PA10, not the remapped PB6/PB7 this firmware
uses for diagnostics:

    adapter RX  <---- PA9   (board TX, ROM bootloader)
    adapter TX  ----> PA10  (board RX, ROM bootloader)
    adapter GND <---> GND
    (adapter must be set to 3.3 V logic)

Then: set the BOOT0 jumper to 1, tap RESET, and re-run this command.

NOTE: PA9 also drives the door-lock lamp on this board, so that LED must be
unplugged before using the ROM bootloader. Reconnecting the ST-Link is the
easier route.
'@ -ForegroundColor Red
                exit 1
            }
        }

        Write-Host "==> Flashing build/bcm_firmware.bin via $Port @ $Baud baud" -ForegroundColor Cyan
        Write-Host '    (BOOT0 must be 1 and RESET tapped, or the bootloader will not answer)' -ForegroundColor Yellow
        & $py -m stm32loader -p $Port -b $Baud -f F1 -e -w -v 'build/bcm_firmware.bin'
        $rc = $LASTEXITCODE
        if ($rc -eq 0) {
            Write-Host '==> Flash OK. Set BOOT0 back to 0 and press RESET - PC13 should blink at 1 Hz.' -ForegroundColor Green
        }
        else {
            Write-Host "==> Flash FAILED (exit $rc). Check: BOOT0=1, RESET tapped, TX/RX not swapped, GND shared." -ForegroundColor Red
        }
        exit $rc
    }

    # --- QEMU (must be the gnuarmeclipse 2.8 build, see NOTE above) ---------
    $qemu = Require (Find-XPackExe 'qemu-arm' 'qemu-system-gnuarmeclipse.exe') `
            'qemu-system-gnuarmeclipse' 'xpm install --global @xpack-dev-tools/qemu-arm@2.8.0-13.1'

    $qemuArgs = @('-board', 'BluePill', '-image', $Elf, '--nographic')

    if ($Task -eq 'run') {
        Write-Host "==> Running on QEMU BluePill (STM32F103C8T6). Ctrl+C to stop." -ForegroundColor Cyan
        if ($Seconds -gt 0) {
            $p = Start-Process -FilePath $qemu -ArgumentList $qemuArgs -PassThru -NoNewWindow
            Start-Sleep -Seconds $Seconds
            try { Stop-Process -Id $p.Id -Force -ErrorAction Stop } catch {}
            Write-Host "`n==> Stopped after $Seconds s." -ForegroundColor Cyan
        }
        else { & $qemu @qemuArgs }
        exit 0
    }

    # --- debug --------------------------------------------------------------
    $qemuArgs += @('-gdb', "tcp::$GdbPort", '-S')
    Write-Host "==> QEMU halted at reset, waiting for GDB on localhost:$GdbPort" -ForegroundColor Cyan

    if (-not $Attach) {
        Write-Host '    Now press F5 in VS Code (folder must be "firmware").' -ForegroundColor Yellow
        & $qemu @qemuArgs
        exit 0
    }

    $gdb = Require (Find-XPackExe 'arm-none-eabi-gcc' 'arm-none-eabi-gdb.exe') `
           'arm-none-eabi-gdb' 'reinstall the arm-none-eabi-gcc xPack'
    $q = Start-Process -FilePath $qemu -ArgumentList $qemuArgs -PassThru -WindowStyle Hidden
    try {
        Start-Sleep -Seconds 2
        Write-Host '==> Attaching console GDB (type "continue", "bt", "quit")' -ForegroundColor Cyan
        & $gdb -ex "target remote localhost:$GdbPort" -ex 'set architecture arm' -ex 'break main' $Elf
    }
    finally { try { Stop-Process -Id $q.Id -Force -ErrorAction Stop } catch {} }
}
finally { Pop-Location }
