# Windows COM8 Safe Serial Workflow

Issue: https://github.com/n30nex/SigurdOS-tdeck-fork/issues/11

This page describes the conservative way to open a T-Deck on Windows when the
device appears as `COM8`. The goal is log capture without accidentally pulling
the ESP32-S3 into ROM download mode.

## Why this exists

Some Windows serial tools assert or pulse DTR and RTS when a port opens. On an
ESP32-S3 board, those line states can combine with reset or boot strapping and
leave the chip at the ROM loader prompt instead of the running application.

The helper in this repo opens the port with DTR and RTS deasserted by default,
with hardware flow control disabled, and it does not write anything to the
device. This reduces the risk of a reset-mode transition during log capture.
It cannot guarantee that every USB bridge driver will avoid every short
DTR/RTS glitch during open.

## One-time setup

Install PySerial if it is not already available:

```powershell
python -m pip install pyserial
```

## Capture COM8 Logs

Start from a normal boot. Do not hold BOOT, RESET, or the trackball button while
plugging in USB unless you intentionally want download mode for flashing.

Dry-run the settings first:

```powershell
python scripts\validation\safe_serial_monitor.py --port COM8 --dry-run
```

Capture 30 seconds of raw serial output:

```powershell
python scripts\validation\safe_serial_monitor.py `
  --port COM8 `
  --baud 115200 `
  --duration 30 `
  --output .pio\serial-captures\com8-boot.log
```

For a live monitor, omit `--duration` and press `Ctrl+C` when done:

```powershell
python scripts\validation\safe_serial_monitor.py --port COM8 --baud 115200
```

The helper is read-only. It does not send a newline, command, reset request, or
upload handshake. The output file is a raw byte capture, so it preserves CR/LF
and any non-UTF-8 bytes.

## Remote-Test Smoke

Only use remote-test mode after explicit approval, because that firmware build
disables the LoRa radio and turns the device into a serial-controlled test
target.

When remote-test mode has already been approved and flashed, the smoke helper
also uses the safe COM8 defaults:

```powershell
python scripts\validation\remote_test_smoke.py --port COM8 --profile telemetry
```

Leave `--dtr` and `--rts` unset. Those flags exist only for an intentional,
documented bench test where asserting a control line is part of the plan.

## What Not To Do

- Do not use `pio device monitor`, Arduino Serial Monitor, PuTTY, Tera Term, or
  other generic terminals for COM8 log capture until you have checked their
  DTR/RTS behavior.
- Do not run `pio run -t upload`, `esptool`, or any upload helper when you only
  need logs.
- Do not hold BOOT, RESET, or the trackball button while opening the serial
  monitor unless flashing is the explicit goal.
- Do not repeatedly open and close COM8 while debugging a boot issue. Keep one
  safe monitor open, capture the evidence, then close it.
- Do not send commands or newlines to unknown firmware. First capture passive
  logs and verify what is running on the device.
- Do not assume `COM8` is always a SigurdOS T-Deck. Check Device Manager and
  the device screen before treating it as the target.

## Recovery If ROM Download Mode Appears

Symptoms can include a black screen, no normal SigurdOS UI, or serial text like
`ESP-ROM` and `waiting for download`.

1. Close the serial monitor or upload tool.
2. Unplug USB.
3. Make sure BOOT, RESET, and the trackball button are released.
4. Plug USB back in and let the board boot normally.
5. If the device still returns to ROM download mode, tap RESET once or power
   cycle the board.
6. Do not flash firmware as a recovery step unless that was already the planned
   operation.

## Notes For Hardware Evidence

- Include the command line used, the capture file path, and whether DTR/RTS
  were left off.
- If no text appears but the device UI is running, keep the capture as evidence
  and do not toggle DTR/RTS without an explicit reason.
- Prefer GitHub Actions artifacts for firmware builds. This workflow is only
  for serial log capture from an already-running device.
