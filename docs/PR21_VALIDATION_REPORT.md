# PR 21 Validation Report

Branch: `codex/fix-tdeck-radio-keyboard-validation`
PR: https://github.com/n30nex/SigurdOS-tdeck-fork/pull/21
Latest candidate commit: `d759a3dd8d3ecd85d3269c11186a489e3725b01d`

## Current Status

PR #21 remains a draft because user-visible confirmation is still pending for
the most recent chat/DM persistence, room server, and repeater-login behavior.

The current candidate commit is code-reviewed, native-tested, built by GitHub
Actions, downloaded from the Actions artifact, and flashed to the COM8 T-Deck
Plus. The latest flash reached firmware startup in a short serial capture, but
the user-visible acceptance flows still need to be confirmed on the device UI
before the PR can be marked ready or related issues can be closed.

## GitHub Actions Evidence

- Pull Request CI run: `28712488504`
- Pull Request CI result: passed
- Build & Release run: `28712843208`
- Build & Release result: passed
- Firmware version: `beta-0.1.43-RC5`
- Manifest git SHA: `d759a3dd8d3e`
- Manifest dirty flag: `false`
- MeshCore SHA: `60ea4a91bf14`
- Built at UTC: `2026-07-04T16:48:55Z`
- Full image SHA256:
  `82e6d45adba6fb8907bbf639cc49547449a751b55d358748e034a21ac98967fc`
- Downloaded artifact path:
  `F:\SIGUI\artifacts\cloud-builds\20260704-125118`
- Artifact structural audit:
  `scripts\audit_launcher_artifact.py` passed against `firmware-merged.bin`
  with `PYTHONIOENCODING=utf-8`

## Hardware Evidence

Target: T-Deck Plus on `COM8`

Flash command used:

```powershell
python -m esptool --chip esp32s3 --port COM8 --baud 921600 --before default_reset --after hard_reset write_flash 0x0 "F:\SIGUI\artifacts\cloud-builds\20260704-125118\firmware-merged.bin"
```

Flash result:

- ESP32-S3 detected on `COM8`
- esptool verified written data
- short boot capture reached firmware startup
- boot capture included `[mesh] Radio not configured - holding SX1262 in reset`

Passive crash monitors:

| Label | Duration | Result |
| --- | ---: | --- |
| `post-flash-b8bc065-idle` | 90s | `raw_bytes=0`, `event_count=0`, exit 0 |
| `d759a3d-idle-serial-120s` | 120s | `raw_bytes=0`, `event_count=0`, exit 0 |

Latest idle monitor report:
`F:\SIGUI\artifacts\cloud-builds\20260704-125118\validation\d759a3d-idle-serial-120s.json`

Guided release-firmware UI journey:

| Profile | Steps | Duration | Result |
| --- | ---: | ---: | --- |
| `chat-public-login` | 7 | 351s | pass, `reset_or_crash_detected=false` |

Journey summary:
`.pio\hardware-ui-journeys\b8bc065-chat-public-login\summary.json`

The guided journey is a crash/reset evidence harness. It records one raw serial
log per timed step and fails on reset/crash signatures. It does not itself prove
that the human-visible login result was correct. It was also run on an earlier
candidate commit (`b8bc065`), so the latest `d759a3d` artifact still needs
visible device confirmation.

Additional guided journey available for the latest acceptance pass:

```powershell
python scripts\validation\hardware_ui_journey.py --port COM8 --profile chat-dm-persistence --out-dir .pio\hardware-ui-journeys\d759a3d-chat-dm-persistence
```

This profile covers Public open/click stability, incoming Public unread
behavior, incoming DM toast/unread behavior, immediate DM row appearance,
CHATS-vs-DMs separation, history persistence across reboot, and post-reboot
Public trackball-click stability. The reboot step explicitly allows reset
signatures while still failing on crash signatures.

## Native Test Evidence

Focused host-native validation was run without building firmware locally.

Latest focused suite:

```powershell
pio test -e native_test -f test_chat_config -f test_prefs -f test_companion_protocol -f test_mesh_contract -f test_message_store -f test_mesh_wrapper -f test_ui_contract -f test_navigation_contract -f test_home_screen -f test_repeater_refresh_policy -f test_mesh_messaging -v
```

Result: 276 test cases passed.

Earlier focused suite:

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

- Chat history record layout, legacy `/msgs` fallback, and companion message
  store fallback
- Live DM row refresh while the chat list is already open
- CHATS-vs-DMs filter persistence/routing
- TX power preference normalization
- MeshCore-compatible login response metadata forwarding
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
- Incoming DMs appear in the DMs list immediately, trigger unread/toast
  behavior, and do not appear under the CHATS list.
- Public/channel history and DM history survive reboot.
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
- #47 Chat/DM unread/history persistence

The code and tests are consistent with fixes for these flows. The latest
artifact is built and flashed, but chat persistence, room server behavior, and
repeater management still need visible confirmation from the device UI before
closing the related issues.

## Safety Notes

- Firmware builds for this report used GitHub Actions, not local PlatformIO
  firmware builds.
- Local validation was limited to native tests and serial monitoring.
- Only `COM8` was used for T-Deck flashing/monitoring.
- `COM11`, `COM12`, `COM16`, and `COM29` were not opened for this report.
- The untracked `hardware_audit/` directory was not modified.
