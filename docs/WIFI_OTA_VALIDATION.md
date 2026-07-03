# WiFi And OTA Validation

Issue: https://github.com/n30nex/SigurdOS-tdeck-fork/issues/15

This runbook validates WiFi station setup, AP upload OTA gating, and GitHub OTA
behavior on T-Deck Plus hardware. Use GitHub Actions-built artifacts; do not
use a local firmware build as release evidence.

## Scope

Validate:

- WiFi scan, connect, disconnect, and reconnect with test credentials.
- GitHub OTA branch and prerelease selection.
- Failure messages for missing credentials, bad credentials, and missing PIN.
- AP upload OTA PIN gating.
- Launcher/self-OTA safety gates when Launcher is detected.

Do not run OTA against a production device unless rollback and recovery are
already planned. Keep test WiFi credentials out of public logs.

## Evidence To Capture

- GitHub Actions run URL, artifact name, build SHA, build profile, and Build
  Info screen.
- Test network SSID label, security type, and whether the network has internet.
  Do not publish the password.
- Screenshots or `widgets` output for Settings > WiFi, WiFi Networks, OTA
  Branch, Pre-releases, and OTA dialogs.
- Serial logs captured with [`WINDOWS_COM8_SERIAL.md`](WINDOWS_COM8_SERIAL.md)
  if using Windows/COM8.
- Result table for each case below.

## Preconditions

- Device has a known test PIN for OTA gating.
- Test WiFi credentials are available to the operator.
- Battery is sufficiently charged or USB power is stable.
- The currently running firmware is expected to be SigurdOS before any OTA test
  starts.

## WiFi Station Validation

1. Open Settings > System and capture the `WiFi: <ssid> / Not set` row.
2. Open Settings > WiFi.
3. Start scan and select the test SSID.
4. Enter the test password.
5. Wait for connected state.
6. Reboot the device.
7. Return to Settings > WiFi and confirm reconnect.

Expected:

- Scan lists the test SSID with plausible RSSI/security.
- Wrong password produces a visible failure state.
- Correct password reaches connected state and persists across reboot.
- GitHub OTA rows see the saved credentials.

Fail if the UI accepts credentials but never attempts connection, if it silently
forgets the SSID, or if reconnect requires retyping the password.

## Missing-Credential Gate

Clear or leave WiFi credentials unset, then open Settings > System and press
`OTA from GitHub`.

Expected:

- OTA does not start.
- User-visible status explains that WiFi credentials are missing or unavailable.
- No reboot or flash write occurs.

## GitHub OTA Selection

With test WiFi connected:

1. Cycle `OTA Branch` through `main`, `dev`, and `latest`.
2. Toggle `Pre-releases`.
3. Start `OTA from GitHub` only for the intended test lane.
4. Watch progress until success or a controlled failure.

Expected:

- The dialog label reflects the selected branch/prerelease policy.
- Download progress and final state are visible.
- TLS or HTTP failures produce a readable error.
- On success, the device reboots into the expected Build Info Git SHA/source.

Fail if branch selection is ignored, if the UI becomes unresponsive without a
status message, or if the device reboots into an unexpected artifact.

## AP Upload OTA PIN Gate

With no PIN set:

1. Open Settings > System.
2. Press `OTA Update`.

Expected: AP upload OTA refuses to start and asks for a device PIN first.

With a test PIN set:

1. Press `OTA Update`.
2. Confirm the AP name and upload URL are displayed.
3. From a test client, connect to `SigurdOS-OTA`.
4. Open `http://192.168.4.1`.
5. Try a wrong PIN, then the correct PIN.

Expected:

- Wrong PIN is rejected.
- Correct PIN is required before upload is accepted.
- Do not upload a firmware file unless the test explicitly includes recovery
  and the artifact is known good.

## Launcher Gate

If the device is running under bmorcelli/Launcher:

- Both AP upload OTA and GitHub OTA should refuse to start.
- The UI should direct the user to update through Launcher.
- No self-OTA write should begin.

If Launcher is not detected, record that the gate was not applicable.

## Pass Criteria

The issue is ready to close only when:

- WiFi connect/reconnect succeeds with test credentials.
- Missing-credential and wrong-password cases produce clear failures.
- GitHub OTA branch/prerelease behavior is proven or a focused bug is opened.
- AP upload OTA is PIN-gated.
- Launcher self-OTA protection is validated or explicitly not applicable.

