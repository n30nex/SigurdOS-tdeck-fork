# PR 21 Validation Report

Branch: `codex/fix-tdeck-radio-keyboard-validation`
PR: https://github.com/n30nex/SigurdOS-tdeck-fork/pull/21
Latest candidate commit: `a58dda1f45de26526bfc8c12657b55a3a23fc2b9`

## Current Status

PR #21 remains a draft because visible device acceptance is still pending for
the latest login, notification, persistence, and RF feedback flows.

The current candidate is code-reviewed, native-tested, built by GitHub Actions,
downloaded from the Actions artifact, flashed to the COM8 T-Deck Plus, and
passively monitored after flash without serial crash/reset output. This proves
the current firmware image boots quietly after flashing, but it does not prove
manual UI behavior such as repeater password login, audible buzzer output, or
chat history persistence.

## GitHub Actions Evidence

- Pull Request CI run: `28721412348`
- Pull Request CI result: passed
- Pull Request CI head SHA:
  `a58dda1f45de26526bfc8c12657b55a3a23fc2b9`
- Build Validation Matrix run: `28721855464`
- Build Validation Matrix result: passed
- Build Validation Matrix head SHA:
  `a58dda1f45de26526bfc8c12657b55a3a23fc2b9`
- Downloaded artifact path:
  `F:\SIGUI\artifacts\cloud-builds\20260704-183917-canada-a58dda1`
- `firmware-merged.bin` size: `2,690,864` bytes
- `firmware-merged.bin` SHA256:
  `57F89E7AFBF004F13770B58D7E59551A243063FA8EF9697024E9B6D719D58D6D`
- `firmware.bin` size: `2,625,328` bytes
- `firmware.bin` SHA256:
  `C335E728B70286792BF11CED30C5B11280A2B0E4A279BD8909D81914B49D701E`
- Artifact structural audit:
  `scripts\audit_launcher_artifact.py` passed against `firmware-merged.bin`
  with `PYTHONIOENCODING=utf-8`

The fastest current artifact path is the manual `Build Validation Matrix`
workflow and the `firmware-SigurdOS_TDeck` artifact. It produces the normal
firmware image from GitHub Actions in about three minutes. PR CI builds firmware
but does not upload a flashable artifact.

## Hardware Evidence

Target: T-Deck Plus on `COM8`

COM enumeration before flash showed `COM8` as a USB serial device with
`VID_303A&PID_1001`. `COM11`, `COM12`, `COM16`, and `COM29` were not opened.

Flash command used:

```powershell
python -m esptool --chip esp32s3 --port COM8 --baud 921600 --before default_reset --after hard_reset write_flash 0x0 "F:\SIGUI\artifacts\cloud-builds\20260704-183917-canada-a58dda1\firmware-merged.bin"
```

Flash result:

- ESP32-S3 detected on `COM8`
- MAC: `cc:8d:a2:0d:14:28`
- esptool wrote `2,690,864` bytes
- esptool verified the written hash

Passive crash monitors:

| Label | Duration | Result |
| --- | ---: | --- |
| `a58dda1-com8-postflash` | 30s | raw log length 0 |
| `a58dda1-postflash-idle` | 90s | `raw_bytes=0`, `event_count=0`, exit 0 |

Latest idle monitor report:
`F:\SIGUI\artifacts\cloud-builds\20260704-183917-canada-a58dda1\validation\a58dda1-com8-idle-crash-monitor-90s.json`

The passive monitor keeps DTR and RTS off. It is crash/reset evidence only; it
does not assert that UI actions were performed or visible behavior was correct.

## Native Test Evidence

Focused host-native validation was run without building firmware locally.

Latest focused suite before commit `a58dda1`:

```powershell
git diff --check
pio test -e native_test -f test_ui_timing -f test_mesh_contract -f test_mesh_wrapper -f test_message_store -f test_companion_protocol -f test_chat_config -v
```

Result: `git diff --check` passed and the focused suite passed `186/186`.

PR CI run `28721412348` then passed the repository native test job on
`a58dda1`.

Covered areas include:

- CLI data text subtype preservation
- Login password policy and local login-state clearing
- Room, repeater, and pending-login safety helpers
- Activity flash without non-message buzzer noise
- Incoming message flash and buzzer policy independent of activity sequence
- Message-store record typing and persistence contracts
- Companion protocol message sync contracts

## Still Pending

These items require visible current-head hardware confirmation before closing
the related issues or marking the PR ready:

- #35: Blank repeater login fails fast without crashing, password login does
  not crash, and pending login can be cancelled/backed out.
- #117: Room admin-password login can be cancelled/backed out without trapping
  or rebooting the device.
- #116: Settings System Test Buzzer is audible, incoming Public and DM messages
  use the expected audible patterns, and quiet mode suppresses sound.
- #47: Incoming Public and DM histories plus unread state survive reboot.
- #64: Advertise, Packets, and Signal show a complete RF feedback loop with
  packet metadata plus RSSI/SNR evidence.
- #45: A full release-firmware UI journey has reset/crash monitoring and visual
  evidence for the high-risk screens.

## Issue Disposition

Already closed from current or earlier hardware evidence:

- #32 Opening Chat can leave T-Deck unresponsive to trackball and touch
- #33 Trackball click on Public in Chat page reboots device
- #37 Room server adverts appear but room login, send, and fetch flows do not work
- #38 Repeater detail page crashes and cannot navigate back

Do not close these issues from current automated evidence alone:

- #35 Repeater login fails from T-Deck
- #47 Chat/DM unread/history persistence
- #64 Advertise, Packets, and Signal RF feedback loop validation
- #75 Preserve TXT_TYPE_CLI_DATA through persistence and companion sync
- #116 Incoming message flash works but buzzer is not audible
- #117 Room admin password login crashes while pending

The code and tests are consistent with fixes for several of these flows. The
latest artifact is built, validated, and flashed, but the visible device flows
above still require confirmation before closure.

## Safety Notes

- Firmware builds for this report used GitHub Actions, not local PlatformIO
  firmware builds.
- Local validation was limited to native tests, artifact inspection, flashing,
  and passive serial monitoring.
- Only `COM8` was used for T-Deck flashing/monitoring.
- `COM11`, `COM12`, `COM16`, and `COM29` were not opened for this report.
- The untracked `hardware_audit/` directory was not modified.
