# PR 21 Validation Report

Branch: `codex/fix-tdeck-radio-keyboard-validation`
PR: https://github.com/n30nex/SigurdOS-tdeck-fork/pull/21
Latest flashed firmware candidate commit:
`6af019c6f7b0eb755314a84a72589350a2683fda`

## Current Status

PR #21 remains a draft because the newest room/repeater login hardening needs
visible device acceptance before the related issues can be closed. The current
candidate is native-tested, built by GitHub Actions, downloaded from the
Actions artifact, flashed to the COM8 T-Deck Plus, and passively monitored
after flash without reset/crash signatures.

This proves the current firmware image builds, flashes, and idles after
flashing. It does not prove physical UI or RF behavior unless listed under
confirmed hardware evidence.

## GitHub Actions Evidence

- Pull Request CI run: `28726901624`
- Pull Request CI result: passed
- Pull Request CI head SHA:
  `6af019c6f7b0eb755314a84a72589350a2683fda`
- Build Validation Matrix run: `28727195233`
- Build Validation Matrix result: passed
- Build Validation Matrix head SHA:
  `6af019c6f7b0eb755314a84a72589350a2683fda`
- Downloaded artifact path:
  `F:\SIGUI\artifacts\cloud-builds\20260704-224230-canada-6af019c`
- `firmware-merged.bin` size: `2,694,784` bytes
- `firmware-merged.bin` SHA256:
  `4BD7E4B883744B7F19CDBCF7E715F1D283D0E0F5434D556C60B6E3A78369B156`
- `firmware.bin` size: `2,629,248` bytes
- `firmware.bin` SHA256:
  `DD5DC1A4FAB4D4770C911DDF53AB4A886E3BEAD4DCD62C541F950DDBFA75E2F1`
- Artifact structural audit:
  `scripts\audit_launcher_artifact.py` passed against `firmware-merged.bin`
  with `PYTHONIOENCODING=utf-8`

The fastest current artifact path is the manual `Build Validation Matrix`
workflow and the `firmware-SigurdOS_TDeck` artifact. It produced the normal
firmware image from GitHub Actions in about three minutes. PR CI builds firmware
but does not upload a flashable artifact.

## Hardware Evidence

Target: T-Deck Plus on `COM8`

COM enumeration before flash showed `COM8` as a USB serial device. `COM11`,
`COM12`, `COM16`, and `COM29` were not opened.

Flash command used:

```powershell
python -m esptool --chip esp32s3 --port COM8 --baud 921600 --before default-reset --after hard-reset write-flash 0x0 "F:\SIGUI\artifacts\cloud-builds\20260704-224230-canada-6af019c\firmware-merged.bin"
```

Flash result:

- ESP32-S3 detected on `COM8`
- MAC: `cc:8d:a2:0d:14:28`
- esptool wrote `2,694,784` bytes
- esptool verified the written data hash

Passive post-flash monitor:

- Script: `scripts\validation\ui_crash_monitor.py`
- File: `F:\SIGUI\artifacts\cloud-builds\20260704-224230-canada-6af019c\post-flash-idle-com8.json`
- Duration: `30` seconds
- DTR/RTS: `false` / `false`
- Raw bytes captured: `0`
- Reset/crash events: `0`
- Exit code: `0`

## Native Test Evidence

Focused host-native validation was run without building firmware locally.

Local checks before commit `6af019c`:

```powershell
git diff --check
pio test -e native_test -f test_mesh_contract -f test_repeater_refresh_policy -f test_ui_timing -f test_navigation -f test_ui_contract -v
pio test -e native_test -f test_companion_protocol -f test_chat_config -f test_mesh_contract -f test_mesh_wrapper -f test_repeater_refresh_policy -v
pio test -e native_test -f test_mesh_contract -f test_mesh_messaging -f test_mesh_wrapper -f test_repeater_refresh_policy -f test_companion_protocol -v
pio test -e native_test -v
```

Results:

- `git diff --check`: passed
- Focused UI/login suite: `86/86` passed
- Broader mesh/chat suite: `177/177` passed
- Mesh/login messaging suite: `218/218` passed
- Full native suite: `983` test cases, `1` skipped, `982` succeeded

PR CI run `28726901624` then passed the repository native test job, firmware
build job, and advisory static/logging job on `6af019c`.

## Current Code Changes Under Test

- `parseLoginResponse()` accepts short successful login responses where
  `data[4] == 0` and the newer optional keepalive/permission/ACL fields are
  absent.
- Failed login records are reclaimable, preventing repeated room/repeater
  login failures from poisoning the fixed-size login table.
- Room server and repeater login/admin dialogs now use LVGL current-target
  event lookup for button-owned user data.
- Contact removal dialog ownership was hardened against delete-event bubbling
  and stale heap pointer reuse.
- Repeater reboot confirmation dialog uses current-target parent lookup.

## Confirmed Hardware Evidence From Current Manual Testing

The following items were confirmed by direct device testing before this commit
and remain listed as current issue disposition context:

- Topbar five blue dots were removed and `G W B` appears.
- Contacts -> select contact -> send DM works without reboot.
- Repeater adverts now show non-zero RSSI/SNR.
- Opening Public and RX while Public is open was reported fixed before the
  latest login hardening; retest on `6af019c` is still requested because the
  device was reflashed.

## Still Pending

These items require visible current-head hardware confirmation before closing
the related issues or marking the PR ready:

- #121: Blank `Krabs Lagoon` login opens the room chat directly and does not
  reboot.
- #117: Room admin-password login reaches a cancellable pending/failure/success
  state without rebooting.
- #36: Repeater login does not freeze/reboot; pending can be cancelled/backed
  out; status/telemetry/neighbours/time/adverts are checked one by one.
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
- #41 Topbar five blue dots removal / GWB status indicator
- #118 Repeater RSSI/SNR values all zero
- #119 Public-open RX message black screen/reboot
- #120 Contact detail DM send reboots

Do not close these issues from automated evidence alone:

- #36 Add repeater management page and flow
- #47 Chat/DM unread/history persistence
- #64 Advertise, Packets, and Signal RF feedback loop validation
- #75 Preserve TXT_TYPE_CLI_DATA through persistence and companion sync
- #116 Incoming message flash works but buzzer is not audible
- #117 Room admin password login crashes while pending
- #121 Opening Krabs Lagoon room from Chat can reboot device

The code and tests are consistent with fixes for several of these flows. The
latest artifact is built, validated, flashed, and passively monitored, but the
visible device flows above still require confirmation before closure.

## Safety Notes

- Firmware builds for this report used GitHub Actions, not local PlatformIO
  firmware builds.
- Local validation was limited to native tests, artifact inspection, flashing,
  and passive serial monitoring.
- Only `COM8` was used for T-Deck flashing/monitoring.
- `COM11`, `COM12`, `COM16`, and `COM29` were not opened for this report.
- The untracked `hardware_audit/` directory was not modified.
