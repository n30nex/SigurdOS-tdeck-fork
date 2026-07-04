# PR 21 Validation Report

Branch: `codex/fix-tdeck-radio-keyboard-validation`
PR: https://github.com/n30nex/SigurdOS-tdeck-fork/pull/21
Latest validated commit: `413fb177f21fbfe2bc3eb8edc987b8f4c19ba4b5`

## Current Status

PR #21 remains a draft because user-visible confirmation is still pending for
the most recent room server and repeater-login behavior.

The current commit is code-reviewed, native-tested, built by GitHub Actions,
flashed to the COM8 T-Deck Plus, and passively monitored for reset/crash
signatures. The passive monitor did not observe a reboot, panic, assertion, or
backtrace. That does not prove the physical UI flows are fixed; it only proves
no serial reset signature appeared during the monitored windows.

## GitHub Actions Evidence

- Pull Request CI run: `28705374638`
- Pull Request CI result: passed
- Build & Release run: `28705729626`
- Build & Release result: passed
- Firmware version: `beta-0.1.43-RC5`
- Manifest git SHA: `413fb177f21f`
- Manifest dirty flag: `false`
- Full image SHA256:
  `62c532c7a2b0aaa6d58f13d57fbe600f5759e27d265926b66374394247a6c42c`
- Downloaded artifact path:
  `F:\SIGUI\artifacts\cloud-builds\20260704-0817-github-actions-28705729626\sigurdos-tdeck-firmware`

## Hardware Evidence

Target: T-Deck Plus on `COM8`

Flash command used:

```powershell
python -m esptool --chip esp32s3 --port COM8 --baud 921600 write_flash 0x0 "F:\SIGUI\artifacts\cloud-builds\20260704-0817-github-actions-28705729626\sigurdos-tdeck-firmware\sigurdos-tdeck-full.bin"
```

Flash result:

- ESP32-S3 detected on `COM8`
- MAC: `cc:8d:a2:0d:14:28`
- esptool verified written data
- hard reset completed through RTS after flashing

Passive crash monitors:

| Label | Duration | Result |
| --- | ---: | --- |
| `post-flash-413fb17-idle` | 90s | `raw_bytes=0`, `event_count=0`, exit 0 |
| `manual-chat-public-room-login-413fb17` | 300s | `raw_bytes=0`, `event_count=0`, exit 0 |

## Native Test Evidence

Focused host-native validation was run without building firmware locally:

```powershell
pio test -e native_test -f test_chat_config -f test_mesh_contract -f test_mesh_messaging -f test_repeater_refresh_policy -f test_navigation -f test_navigation_contract -f test_ui_contract -f test_channel_validation -v
```

Result: 199 test cases passed.

The final focused interop guard suite was also rerun:

```powershell
pio test -e native_test -f test_mesh_contract -f test_mesh_messaging -f test_chat_config -f test_channel_validation -v
```

Result: 157 test cases passed.

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
- Room fetch/read behavior works with an actual room server, or produces a
  visible failure state that can be captured and triaged.

## Issue Disposition

Do not close these issues from current evidence alone:

- #32 Opening Chat can leave T-Deck unresponsive to trackball and touch
- #33 Trackball click on Public in Chat page reboots device
- #35 Repeater login fails from T-Deck
- #37 Room server adverts appear but room login, send, and fetch flows do not work
- #38 Repeater detail page crashes and cannot navigate back

The code and tests are consistent with fixes for these flows, and COM8 crash
monitoring did not catch a reset during the manual chat/login window. The room
and repeater login behavior still needs visible confirmation from the device UI
before closing the related issues.

## Safety Notes

- Firmware builds for this report used GitHub Actions, not local PlatformIO
  firmware builds.
- Local validation was limited to native tests and serial monitoring.
- Only `COM8` was used for T-Deck flashing/monitoring.
- `COM11`, `COM12`, `COM16`, and `COM29` were not opened for this report.
- The untracked `hardware_audit/` directory was not modified.
