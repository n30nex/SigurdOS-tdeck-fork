# RF Interop Test Plan

Minimal local-node RF validation for SigurdOS T-Deck. This plan covers
repeaters, room servers, DMs, channel messages, packet-log evidence, trace/path,
and reboot persistence without sending repeated traffic onto a public mesh.

Use firmware artifacts built by GitHub Actions. Do not use a local firmware
build as release evidence for this plan.

## Scope

This is an opt-in field test for a controlled local mesh. It is not a broad
soak test, certification test, or public-network compatibility claim.

Validate only with nodes you control:

- DUT: SigurdOS T-Deck running a GitHub Actions artifact.
- Peer chat node: a second MeshCore-compatible node for adverts, DMs, ACKs, and
  channel messages.
- Local repeater: a controlled repeater node for advert, login/status, and
  trace/path checks.
- Local room server: a controlled room server for login and message fetch.

Use unique names for the run, for example `SigDUT-0703`, `Peer-0703`,
`Rpt-0703`, and `Room-0703`, so screenshots and packet logs are unambiguous.

## Safety

Prefer the RX-only lane first. The
`SigurdOS_TDeck_remote_test_radio_usca_rxonly` profile enables the radio on the
USA/Canada tuple and defines `SIGURDOS_REMOTE_TEST_RX_ONLY=1`, so DUT transmit
commands are blocked.

RX-only can validate receive path, contact auto-add from other local nodes,
incoming DM/channel rendering, and packet-log classification. It cannot validate
DUT advert transmit, DUT DM transmit, real DM ACK, channel transmit, repeater
login/status, room fetch, or trace requests.

Switch to a transmit-capable profile only when all of these are true:

- The test is on a legal local frequency and region profile.
- The peer nodes are local and controlled.
- The run is rate-limited to one advert, one DM, one channel message, and one
  status/fetch/trace request per target unless a maintainer explicitly asks for
  a retry.
- The operator records the start/end time and stops on the first unexpected
  repeated transmit.

Do not use public repeaters or public room servers for this test. Do not count
the test controller's `(ACK simulated)` line as RF ACK evidence; it exists for UI
verification. A real RF ACK must be shown by peer logs and/or an `ACK` entry in
the DUT Heard/Packets screen.

## Evidence To Capture

Capture these items in the issue or PR:

- GitHub Actions run URL, artifact name, build SHA, and build profile.
- DUT serial port, peer node names, room/repeater names, and RF tuple.
- Serial transcript from the DUT test-controller session.
- Peer-node transcript or screenshots proving receive/ACK/fetch/status actions.
- `widgets` output or a screenshot/capture for `heard`, `chat`, `repeaters`,
  `contactdetail`, `nodestats`, `nodestatus`, and `trace` screens as applicable.
- Packet counters before and after the active transmit lane.
- Pass/fail table with each case ID below.

For screen evidence, use:

```text
nav <screen>
widgets
capture
```

`capture` emits framebuffer hex and is useful when a visual artifact is needed.
`widgets` is usually enough for text evidence.

## Preflight

Connect to the DUT serial test controller at 115200 baud after flashing the
Actions-built artifact.

Run these commands before any RF action:

```text
help
debug 1
getrf
contactstats
nav nodestats
widgets
nav heard
widgets
nav terminal
term-submit status
term-log
```

Expected observations:

- `help` lists the remote test controller commands.
- `getrf` prints the profile name, expected frequency, spreading factor,
  bandwidth, coding rate, TX power, channel count, noise floor, RSSI/SNR,
  airtime, packet counters, and packet-log count for the artifact.
- RX-only builds also print `remote-test RX-only mode enabled; TX commands are
  blocked`.
- `contactstats` gives the starting stored/exported/repeater/room/chat counts.
- `nodestats`, `heard`, and terminal `status` provide baseline counters and
  radio state before any transmit-capable command is used.

Fail if the RF tuple is wrong, the DUT is not the expected Actions SHA/profile,
or the baseline screens cannot be captured.

## RX-Only Lane

Use this lane with `SigurdOS_TDeck_remote_test_radio_usca_rxonly`.

### RX0: Blocked-TX Sanity

Commands:

```text
getrf
advert
sendchannel testingsigurdos rxonly-should-not-send
nav heard
widgets
```

Expected observations:

- `getrf` reports RX-only mode.
- `advert` returns `FAILED`.
- `sendchannel` returns `FAILED` or no send confirmation.
- Heard/Packets does not gain new DUT `TX_ADV` or `TX_CHAN` rows from these
  blocked commands.

Fail if any blocked command creates verified over-the-air traffic.

### RX1: Contact Auto-Add And Packet Log

Peer action: send one advert from the local peer node.

DUT commands:

```text
contactstats
nav heard
widgets
nav contacts
widgets
```

Expected observations:

- `contactstats` chat count increases or the peer appears in Contacts.
- Heard/Packets shows an advert-style RX row, RSSI, and SNR for the peer.
- The peer name and timestamp are recorded in the evidence.

Pass if the peer is visible as a contact and the packet log proves a received
packet from the peer. Fail if the peer sent the advert but no contact/log entry
appears after a reasonable receive window.

### RX2: Incoming DM And Channel Message

Prepare the local channel on the DUT. This is safe in RX-only because it does
not transmit:

```text
addchannel testingsigurdos Si/tjXzmnwmPBA43Fw4b3Q==
```

Peer action: send one DM to the DUT and one channel message on the matching
local test channel.

DUT commands:

```text
nav heard
widgets
opendm Peer-0703
widgets
nav chat
widgets
```

Expected observations:

- Heard/Packets shows DM and group/channel RX rows.
- The DM conversation contains the peer's exact test text.
- The chat/channel view contains the peer's exact channel text.

Pass if both message bodies are visible on the DUT and the packet log shows the
corresponding receive activity. Fail if UI injection is the only evidence.

## Transmit Lane

Use a transmit-capable GitHub Actions artifact only after the safety checks
above. For USA/Canada validation use `SigurdOS_TDeck_remote_test_radio_usca`.
For a controlled room-server fixture that expects the 869.525 MHz/SF11/BW250
tuple, use `SigurdOS_TDeck_remote_test_radio_roomtest` instead.

### TX0: Active Baseline

Commands:

```text
getrf
debug mesh 1
contactstats
nav nodestats
widgets
nav heard
widgets
```

Expected observations:

- `getrf` does not print the RX-only warning and captures the active profile,
  channel count, live signal metrics, airtime, and packet counters before TX.
- Baseline node stats and packet log are captured before transmit.
- Existing contact, repeater, and room counts are known.

Fail if the RF tuple does not match the local peer setup.

### TX1: One Advert

Command:

```text
advert
```

Peer evidence:

- The peer chat node or repeater sees `SigDUT-0703` as a received advert.

DUT evidence:

```text
nav heard
widgets
contactstats
```

Pass if the peer sees exactly one DUT advert and the DUT packet log records
`TX_ADV`. Fail if the DUT sends repeated adverts, the peer never sees the DUT,
or the advert appears on an unintended public node.

### TX2: DM Send, Real ACK, And Reply

Prerequisite: the peer must already be present as a real RF contact from adverts.

Command:

```text
senddm Peer-0703 rf-dm-0703-001
```

Peer action: confirm receipt, then send one DM reply to the DUT.

DUT evidence:

```text
nav heard
widgets
opendm Peer-0703
widgets
```

Expected observations:

- Serial prints `sendmessage OK`.
- Peer log shows `rf-dm-0703-001` received from the DUT.
- DUT Heard/Packets shows `TX_DM` and a real `ACK` RX row, or peer logs prove
  it sent the ACK.
- DUT DM view shows the peer's reply.

Pass only with peer receive evidence and real ACK evidence. The controller's
`(ACK simulated)` line is not sufficient.

### TX3: Channel Send And Receive

Prepare the shared channel on DUT and peer. The PSK below is the automation
channel used by remote-test radio profiles:

```text
addchannel testingsigurdos Si/tjXzmnwmPBA43Fw4b3Q==
sendchannel testingsigurdos rf-chan-0703-001
```

Peer action: confirm receipt, then send one channel reply on the same channel.

DUT evidence:

```text
nav heard
widgets
nav chat
widgets
```

Expected observations:

- Serial prints `sendchannel OK`.
- Peer log shows `rf-chan-0703-001` received.
- DUT Heard/Packets shows `TX_CHAN` and the peer's group/channel RX row.
- DUT chat view shows the peer's channel reply.

Pass if transmit and receive are both proven by peer logs and DUT packet/chat
evidence. Fail if only local UI state changed.

### TX4: Room Server Login And Post

Prerequisite: the room server is discovered over RF. Use `addroomserver` only as
a dry UI rehearsal; it does not prove RF discovery.

Commands:

```text
contactstats
login Room-0703 <password>
loginstat Room-0703
nav chat
widgets
nav heard
widgets
```

Expected observations:

- `contactstats` reports at least one room server before login.
- `login` returns `OK` or a pending state that later becomes `status=2` in
  `loginstat`.
- The room-server post path accepts a test message or shows a clear visible
  failure state.
- Chat shows the room post state, and Heard/Packets and/or peer room-server
  logs show the room-server interaction.

Pass if login state, room post behavior, and a visible response/failure state
are all captured. Fail if the room was only injected locally or if no response
is visible. Stock MeshCore room servers do not currently expose a compatible
fetch/read request, so `fetchmsgs` is intentionally unsupported in the current
release path.

### TX5: Repeater Status Request

Prerequisite: the repeater is discovered over RF. Use `addrepeater` only as a
dry UI rehearsal; it does not prove RF discovery.

Commands:

```text
contactstats
nav repeaters
widgets
nav contactdetail Rpt-0703
widgets
```

Use the `widgets` output to find the visible `Status` row, then tap the center
of that row:

```text
tap <x> <y>
widgets
```

Expected observations:

- `contactstats` reports at least one repeater before the request.
- Contact Detail shows the repeater's RF metadata.
- After the status tap, the Node Status screen shows status rows such as
  Battery, Uptime, Airtime, Last RSSI, Last SNR, Packets, and Flood/Direct.
- Peer repeater logs show one status request from the DUT.

Pass if the Node Status rows and peer status-request evidence are captured.
Fail if the screen stays on `Requesting status...` with no peer-side request.

### TX6: Trace And Path

Prerequisite: the target peer/repeater has a known path or can answer trace
requests.

Commands:

```text
nav trace
widgets
```

Use `widgets` to identify the target row, then tap it:

```text
tap <x> <y>
widgets
nav heard
widgets
```

Expected observations:

- Trace screen shows the target and either `[path known]` or a trace result
  after the request.
- Heard/Packets or peer logs show trace/path traffic.
- Any path length, hop hash, RSSI, or SNR shown by the UI is captured.

Pass if trace/path evidence appears on the DUT or peer. Fail if the target has
no path and no trace-capable response is observed.

### TX7: Reboot Persistence

Capture state before reboot:

```text
contactstats
nav chat
widgets
nav nodestats
widgets
nav heard
widgets
```

Reboot:

```text
reboot
```

After the controller banner returns:

```text
getrf
contactstats
nav chat
widgets
nav contacts
widgets
nav heard
widgets
```

Expected observations:

- RF configuration remains correct.
- Contacts, channels, and chat/message history from the run are still present.
- Packet log evidence from before reboot was already captured. The runtime
  packet log may start fresh after reboot; if so, send or receive one new local
  peer advert and capture the new Heard/Packets entry.

Pass if persistent state survives reboot and fresh packet logging still works.
Fail if contacts/channels/messages disappear without an explicit factory reset.

## Cleanup

The auto-joined `#testingsigurdos` channel in forced remote-radio builds is
RAM-only and is not saved during boot. Channels that were added manually during
the run are persistent by design; remove temporary validation channels before
returning the device to normal use:

```text
removechannel testingsigurdos
```

## Final Pass Criteria

The run passes only when:

- RX-only lane proves passive receive behavior or is explicitly marked not run.
- Transmit lane uses only controlled local nodes and captures one advert, one DM,
  one channel message, one room fetch, one repeater status request, and one trace
  attempt.
- Packet counters or Node Stats are captured before and after transmit testing.
- Heard/Packets contains the expected TX/RX classifications for each RF action.
- Peer logs prove real over-the-air receive/ACK/status/fetch behavior.
- Reboot persistence is checked.
- Any skipped item states why it was skipped, what hardware was missing, and
  which follow-up issue or test slot should cover it.
