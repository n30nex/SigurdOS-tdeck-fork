# BLE Companion Validation

Issue: https://github.com/n30nex/SigurdOS-tdeck-fork/issues/10

This runbook validates the SigurdOS BLE companion bridge against the official
MeshCore app and the host-side protocol smoke script. Use GitHub Actions-built
firmware artifacts; do not use a local build as release evidence.

## Scope

Validate on a T-Deck Plus with the normal firmware or
`SigurdOS_TDeck_ble_validation` Actions artifact:

- BLE advertising and NUS service discovery.
- PIN pairing/security behavior.
- Reconnect after app and device restart.
- Device query, app start, contacts, channels, time, battery/storage, and stats.
- DM/channel message sync with the device's local message store.
- Official MeshCore app interop for send/receive and basic management flows.

This does not replace RF interop testing. For LoRa-side message and repeater
evidence, use [`RF_INTEROP_TEST_PLAN.md`](RF_INTEROP_TEST_PLAN.md).

## Safety

- Use a test device PIN that can be disclosed in the private validation notes,
  not a production PIN.
- Keep private-key import/export out of the happy-path test unless the test is
  specifically about backup/restore.
- Do not capture private message content, real contacts, WiFi passwords, or
  private keys in public artifacts.
- BLE validation may require a phone and host adapter. If either is missing,
  mark the case `blocked`, not `failed`.

## Evidence To Capture

- GitHub Actions run URL, artifact name, build SHA, build profile, and device
  Build Info screen.
- Device name, BLE address or Windows device UUID, app version, phone OS, and
  host OS.
- Pairing result and whether a PIN was requested.
- Host smoke summary JSON and log path from `scripts/validation/companion_ble_smoke.py`.
- Official app screenshots for contacts, channels, sync state, and a test
  message thread.
- Any frame-level failures as separate follow-up issues with command/response
  codes and redacted frame hex when safe.

## Preflight

On the device:

1. Open Settings > Bluetooth.
2. Enable the BLE companion bridge.
3. Record the displayed PIN and device name.
4. Open Settings > System > Build Info and capture the version, Git SHA, build
   source, run id, and ref.

On the host:

```powershell
python -m pip install bleak
python scripts\validation\companion_ble_smoke.py --help
```

On Windows, if pairing by PIN from the script is needed, install the optional
WinRT dependency required by the host environment and use the script's
`--winrt-pin-env` flow.

## Host Protocol Smoke

Run discovery by MeshCore name prefix:

```powershell
python scripts\validation\companion_ble_smoke.py `
  --name-prefix MeshCore- `
  --include-frame-hex
```

If multiple devices are found, rerun with the explicit address/UUID reported by
the first scan:

```powershell
python scripts\validation\companion_ble_smoke.py `
  --address <ble-address-or-windows-uuid> `
  --include-frame-hex
```

Expected:

- Device query returns `RESP_DEVICE_INFO`.
- App start returns `RESP_SELF_INFO`.
- Contacts start returns `RESP_CONTACTS_START`.
- Time, battery/storage, and stats requests return their expected response
  codes.
- Sync returns either `RESP_NO_MORE_MESSAGES` or a queued message frame.
- The output summary is written under `.pio/ble_companion_validation/`.

Fail if the host cannot discover the device, NUS UUIDs are missing, pairing
fails with the known-good PIN, or any required response code is absent.

## Reconnect

Run the smoke script with reconnects:

```powershell
python scripts\validation\companion_ble_smoke.py `
  --address <ble-address-or-windows-uuid> `
  --reconnects 2
```

Expected:

- Every reconnect repeats device query, app start, and sync successfully.
- The official app can reconnect after being force-closed and reopened.
- The T-Deck Bluetooth screen returns to connected/disconnected accurately.

Fail if the device requires forgetting/re-pairing for every reconnect.

## Official App Interop

Use a local test identity and controlled peer nodes.

1. Pair the official MeshCore app to the T-Deck.
2. Confirm the app shows the T-Deck identity and radio parameters.
3. Confirm contacts and channels sync from the T-Deck to the app.
4. Send one DM from the app to a controlled peer contact.
5. Send one channel message from the app to a controlled test channel.
6. Receive one peer DM and one peer channel message, then sync the app.
7. Reboot the T-Deck and reconnect the app.

Expected:

- App-originated messages appear in the T-Deck chat UI and message store.
- T-Deck/peer-originated messages sync to the app without duplication.
- Reconnect after reboot does not lose contacts, channels, or queued messages.

Pass only with app screenshots and T-Deck serial/UI evidence. If app pairing
works but a protocol command fails, open a focused companion-protocol issue with
the failing command, response code, and redacted frame evidence.

