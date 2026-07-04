# Field Validation Checklist

Use this checklist when testing a GitHub Actions firmware artifact on a physical T-Deck Plus. It focuses on evidence that cannot be proven by native tests or CI builds.

Before opening the serial port, recheck that the T-Deck Plus enumerates as `COM8` and use `scripts/validation/safe_serial_monitor.py` or the workflow in [`WINDOWS_COM8_SERIAL.md`](WINDOWS_COM8_SERIAL.md). Do not use generic terminal defaults that assert DTR/RTS.

## UI Crash Monitor

For manual touch/trackball crash reproduction, keep a passive crash monitor open
while the tester performs the UI actions. This does not send input, reset the
device, or switch to remote-test firmware:

```powershell
python scripts\validation\ui_crash_monitor.py `
  --port COM8 `
  --duration 180 `
  --label public-chat-and-repeater-login `
  --output .pio\serial-captures\public-chat-and-repeater-login.raw.log `
  --json-report .pio\serial-captures\public-chat-and-repeater-login.report.json `
  --expect-no-reset
```

The JSON report records `ESP-ROM`, reset, panic, assert, and backtrace
signatures with timestamps. A non-zero exit with `--expect-no-reset` means the
manual flow produced reset/crash evidence that should be attached to the issue.

## Guided Release-Firmware UI Journey

For repeatable issue evidence on the normal release firmware, use the guided
journey harness. It still requires a human to perform the physical touch,
trackball, and keyboard actions, but it records one raw serial log per step and
writes a summary JSON that fails on reset/crash signatures:

```powershell
python scripts\validation\hardware_ui_journey.py `
  --port COM8 `
  --profile chat-public-login `
  --forbid-port COM11 `
  --forbid-port COM12 `
  --forbid-port COM16 `
  --forbid-port COM29 `
  --out-dir .pio\hardware-ui-journeys\pr21-chat-public-login
```

Useful built-in profiles:

- `chat-public-login` covers Chat, Public touch/trackball entry, Krabs Lagoon
  login, local repeater login, and repeater detail back navigation.
- `core-navigation` covers the main Home tiles and the Map/GPS/Settings path.

By default the harness is passive after opening the serial port. It does not
inject UI input, does not reset the board, and does not switch to remote-test
firmware. It also refuses `COM11`, `COM12`, `COM16`, and `COM29` by default so
the known non-target devices are not opened accidentally. Screenshot capture is
optional because release builds normally keep serial commands disabled. Only
enable screenshot attempts when the flashed firmware is known to support
`SCREENSHOT`:

```powershell
python scripts\validation\hardware_ui_journey.py `
  --port COM8 `
  --profile core-navigation `
  --screenshot-mode attempt `
  --out-dir .pio\hardware-ui-journeys\core-navigation-screens
```

Use `--screenshot-mode require` only for a build where screenshot support is a
required part of the test. If screenshot support is unavailable, the raw logs
and summary JSON are still valid crash/no-crash evidence.

## Setup And Radio Input

Evidence to capture:

- Setup wizard node-name entry, date picker, and time picker operated by touch and trackball.
- Setup wizard country radio preset selection for USA and Canada, without requiring manual frequency entry.
- Settings > Radio / Mesh > Radio Setup field edits for preset profile, frequency, SF, bandwidth, coding rate, TX power, and RX boost using touch and trackball.
- A saved configuration reboot, followed by the Radio Setup summary showing the same profile and RF parameters.

Relevant code:

- `src/ui/onboarding_screen.cpp`
- `src/ui/screens/screen_radio_setup.cpp`
- `src/ui/screens/screen_settings_radio.cpp`

## Keyboard Variant Capture

Do not add a hardware remap from guesses. Use Settings > System > Input Self-Test and capture one row per physical key tested.

For each key, record:

- Physical label.
- `km` C3 key-mode byte.
- `out` final output codepoint.
- `mat` five raw matrix bytes.
- `mod` Shift/Ctrl/Alt/Sym/Mic bits.
- Active keyboard layout id.

Cover letters, numbers, punctuation, Space, Enter, Backspace, Shift, Sym, Alt/Mic, double-space layout switching, and Alt+B backlight behavior.

Relevant code:

- `src/hal/keyboard.cpp`
- `src/hal/keyboard.h`
- `src/ui/screens/screen_settings_system.cpp`

## SD Card

Use Settings > System > SD Card.

Evidence to capture:

- Mounted state before and after a rapid reboot or firmware swap.
- Attempt count, last source, last error, and last backoff.
- Free and total size when mounted.
- Result after pressing Retry.
- Whether Map opens after a successful retry.

Relevant code:

- `src/hal/sdcard.cpp`
- `src/ui/screens/screen_settings_system.cpp`
- `src/app/map_renderer.cpp`

## GPS

Use Settings > GPS / Location > GPS diagnostics. Test outdoors or near a window with the antenna positioned consistently.

Evidence to capture:

- Assessment state.
- Fix quality, GSA fix type, RMC status, satellites used, satellites in view, and GSV SNR summary.
- Active baud, characters, received/valid sentences, per-sentence counts, checksum failures, and baud switches.
- Position, altitude, UTC, and sync flag after waiting for acquisition.

Relevant code:

- `src/hal/gps.cpp`
- `src/ui/screens/screen_settings_gps.cpp`

## Map Tiles

Use a known tile pack generated by `scripts/download_maps.py`, then copy the `tiles/` directory to the SD card root.

Evidence to capture:

- Tile pack command and generated `metadata.json`.
- Active radio profile before opening Map.
- SD Card diagnostics mounted state.
- Map first view, pan, zoom in, zoom out, and behavior when panning near tile bounds.
- Whether the map centers from metadata or falls back to USA/Canada profile defaults when no tiles are present.

Relevant code:

- `scripts/download_maps.py`
- `src/app/map_renderer.cpp`
- `src/ui/screens/screen_map.cpp`
