# PR 21 Validation Report

Branch: `codex/fix-tdeck-radio-keyboard-validation`
PR: https://github.com/n30nex/SigurdOS-tdeck-fork/pull/21
Latest validated commit: `b8bc06595ca33f758f0836010b0085c0b0468173`

## Current Status

PR #21 remains a draft because user-visible confirmation is still pending for
the most recent room server and repeater-login behavior.

The current commit is code-reviewed, native-tested, built by GitHub Actions,
flashed to the COM8 T-Deck Plus, and passively monitored for reset/crash
signatures. The passive monitor did not observe a reboot, panic, assertion, or
backtrace. That does not prove the physical UI flows are fixed; it only proves
no serial reset signature appeared during the monitored windows.

## GitHub Actions Evidence

- Pull Request CI run: `28706756200`
- Pull Request CI result: passed
- Build & Release run: `28707116256`
- Build & Release result: passed
- Firmware version: `beta-0.1.43-RC5`
- Manifest git SHA: `b8bc06595ca3`
- Manifest dirty flag: `false`
- Full image SHA256:
  `c92f560834a4a677e148b523806d15321060be982b6b715de98cfaafba5c13ec`
- Downloaded artifact path:
  `F:\SIGUI\artifacts\cloud-builds\20260704-0913-github-actions-28707116256\sigurdos-tdeck-firmware`

## Hardware Evidence

Target: T-Deck Plus on `COM8`

Flash command used:

```powershell
python -m esptool --chip esp32s3 --port COM8 --baud 921600 write_flash 0x0 "F:\SIGUI\artifacts\cloud-builds\20260704-0913-github-actions-28707116256\sigurdos-tdeck-firmware\sigurdos-tdeck-full.bin"
```

Flash result:

- ESP32-S3 detected on `COM8`
- MAC: `cc:8d:a2:0d:14:28`
- esptool verified written data
- hard reset completed through RTS after flashing

Passive crash monitors:

| Label | Duration | Result |
| --- | ---: | --- |
| `post-flash-b8bc065-idle` | 90s | `raw_bytes=0`, `event_count=0`, exit 0 |

Guided release-firmware UI journey:

| Profile | Steps | Duration | Result |
| --- | ---: | ---: | --- |
| `chat-public-login` | 7 | 351s | pass, `reset_or_crash_detected=false` |

Journey summary:
`.pio\hardware-ui-journeys\b8bc065-chat-public-login\summary.json`

The guided journey is a crash/reset evidence harness. It records one raw serial
log per timed step and fails on reset/crash signatures. It does not itself prove
that the human-visible login result was correct; visible device behavior still
needs to be confirmed by the tester.

## Native Test Evidence

Focused host-native validation was run without building firmware locally:

```powershell
pio test -e native_test -f test_chat_config -f test_mesh_contract -f test_mesh_messaging -f test_repeater_refresh_policy -f test_navigation -f test_navigation_contract -f test_ui_contract -f test_channel_validation -v
```

Result: 199 test cases passed on the previous firmware-bearing commit.

The current commit added the guided hardware journey harness and removed visible
stock-room-server fetch affordances. Focused host-native validation was rerun:

```powershell
pio test -e native_test -f test_mesh_contract -f test_ui_contract -f test_controller -f test_terminal -f test_chat_config -f test_repeater_refresh_policy -v
```

Result: 76 test cases passed.

Covered areas:

- Public/channel chat configuration and deferred channel open policy
- Room message formatting, parsing, and room-server text length limits
- Mesh message queue, pending ACK/request behavior, and response parsing
- Navigation/back stack contracts
- Repeater refresh policy
- Shared UI contracts

## Still Pending

These items require visible hardware confirmation before closing the related
issues or marking the PR ready:

- Login to Krabs Lagoon produces visible success, pending, or failure state
  instead of doing nothing.
- Login to the local repeater with a password does not crash.
- Public room post after login is sent through the room-server path and does
  not pollute DM history with the `#Public` transport prefix.
- Stock MeshCore room-server fetch/read stays hidden or fails closed with the
  unsupported message; a compatible fetch protocol is future work.

## Issue Disposition

Do not close these issues from current evidence alone:

- #32 Opening Chat can leave T-Deck unresponsive to trackball and touch
- #33 Trackball click on Public in Chat page reboots device
- #35 Repeater login fails from T-Deck
- #37 Room server adverts appear but room login, send, and fetch flows do not work
- #38 Repeater detail page crashes and cannot navigate back

The code and tests are consistent with fixes for these flows, and COM8 crash
monitoring did not catch a reset during the guided chat/login journey. The room
and repeater login behavior still needs visible confirmation from the device UI
before closing the related issues.

## Safety Notes

- Firmware builds for this report used GitHub Actions, not local PlatformIO
  firmware builds.
- Local validation was limited to native tests and serial monitoring.
- Only `COM8` was used for T-Deck flashing/monitoring.
- `COM11`, `COM12`, `COM16`, and `COM29` were not opened for this report.
- The untracked `hardware_audit/` directory was not modified.
