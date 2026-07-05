# PR 21 Validation Report

Branch: `codex/fix-tdeck-radio-keyboard-validation`
PR: https://github.com/n30nex/SigurdOS-tdeck-fork/pull/21
Latest flashed firmware candidate commit:
`1498d8fb4fb0df7c4c18252177b610362bf5bb33`

## Current Status

PR #21 remains a draft because visible device acceptance is still pending for
room admin login, repeater management login, room chat entry, buzzer behavior,
and persistence/RF validation flows.

The current candidate is code-reviewed, native-tested, built by GitHub Actions,
downloaded from the Actions artifact, flashed to the COM8 T-Deck Plus, and
passively monitored after flash without serial panic/backtrace/reset-loop
output. This proves the current firmware image builds and boots after flashing.
It does not prove manual UI behavior.

## GitHub Actions Evidence

- Pull Request CI run: `28725117689`
- Pull Request CI result: passed
- Pull Request CI head SHA:
  `1498d8fb4fb0df7c4c18252177b610362bf5bb33`
- Build Validation Matrix run: `28725132616`
- Build Validation Matrix result: passed
- Build Validation Matrix head SHA:
  `1498d8fb4fb0df7c4c18252177b610362bf5bb33`
- Downloaded artifact path:
  `F:\SIGUI\artifacts\cloud-builds\20260704-211548-canada-1498d8f`
- `firmware-merged.bin` size: `2,694,416` bytes
- `firmware-merged.bin` SHA256:
  `BF92A338319EDD8BA0A6C01D247E4B32FB702DAEDF50578D91EB9837EEE3C493`
- `firmware.bin` size: `2,628,880` bytes
- `firmware.bin` SHA256:
  `AD6270E828103EBBF732D7026C60DB96653CFFA706DFC1AA362AEE19BEAFBD42`
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
python -m esptool --chip esp32s3 --port COM8 --baud 921600 --before default-reset --after hard-reset write-flash 0x0 "F:\SIGUI\artifacts\cloud-builds\20260704-211548-canada-1498d8f\firmware-merged.bin"
```

Flash result:

- ESP32-S3 detected on `COM8`
- MAC: `cc:8d:a2:0d:14:28`
- esptool wrote `2,694,416` bytes
- esptool verified the written hash

Passive boot capture:

- File: `F:\SIGUI\artifacts\cloud-builds\20260704-211548-canada-1498d8f\boot-capture-com8.txt`
- Raw bytes captured: `321`
- Observed one ROM boot banner and app entry
- No panic, abort, backtrace, or reset loop observed in the capture
- Log includes `[mesh] Radio not configured - holding SX1262 in reset`

The `Radio not configured` line is expected after flashing the merged image,
because the full merged flash resets local configuration state. Setup/Canada
preset must be completed again before RF/manual UI validation.

## Native Test Evidence

Focused host-native validation was run without building firmware locally.

Latest focused suite before commit `1498d8f`:

```powershell
git diff --check
pio test -e native_test -f test_chat_config -f test_mesh_contract -f test_repeater_refresh_policy -f test_build -f test_ui_contract -v
```

Result: `git diff --check` passed and the focused suite passed `83/83`, with
one expected native skip in `test_build`.

Covered areas include:

- Public/room live append visible-budget trimming
- Active room-server context set/clear contract
- Login status cancel policy
- Blank room guest-login refresh policy
- Admin/repeater pending-login detail refresh policy
- Stale delayed login-detail refresh gating
- UI public API/header inclusion contracts

PR CI run `28725117689` then passed the repository native test job, firmware
build job, and advisory static/logging job on `1498d8f`.

## Current Code Changes Under Test

- Blank-password room login opens the selected room chat immediately after the
  guest login request is sent.
- `chat_screen_open_room()` sets the active room server before opening the room
  conversation so room sends route to the selected room server.
- Repeater/detail state and back overrides are explicitly cleared before room
  chat navigation.
- Room admin-password and repeater login paths stay in the detail flow and
  refresh into visible pending/cancel state.
- Delayed login-detail refreshes only rebuild the still-open detail contact.
- Several room/contact/admin modal allocations now fail closed with
  `new(std::nothrow)` guards instead of throwing on low heap.
- Public live message append trims synchronously from the head instead of
  rebuilding the entire visible message list when the render budget is exceeded.

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

Do not close these issues from current automated evidence alone:

- #36 Add repeater management page and flow
- #47 Chat/DM unread/history persistence
- #64 Advertise, Packets, and Signal RF feedback loop validation
- #75 Preserve TXT_TYPE_CLI_DATA through persistence and companion sync
- #116 Incoming message flash works but buzzer is not audible
- #117 Room admin password login crashes while pending
- #121 Opening Krabs Lagoon room from Chat can reboot device

The code and tests are consistent with fixes for several of these flows. The
latest artifact is built, validated, flashed, and passively boot-monitored, but
the visible device flows above still require confirmation before closure.

## Safety Notes

- Firmware builds for this report used GitHub Actions, not local PlatformIO
  firmware builds.
- Local validation was limited to native tests, artifact inspection, flashing,
  and passive serial monitoring.
- Only `COM8` was used for T-Deck flashing/monitoring.
- `COM11`, `COM12`, `COM16`, and `COM29` were not opened for this report.
- The untracked `hardware_audit/` directory was not modified.
