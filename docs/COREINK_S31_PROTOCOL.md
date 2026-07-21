# CoreInk to S31 Protocol

This document defines the control boundary between a CoreInk controller and one
or more Sonoff S31 relay satellites.

The product-level appliance plan lives in `COREINK_OFFLINE_PRODUCT_PLAN.md`.
Device state machines live in `COREINK_S31_STATE_MACHINES.md`.
Exact JSON schemas and invariants live in `COREINK_S31_JSON_SCHEMAS.md`.

The protocol supports two architecture profiles:

- Profile A: CoreInk-authoritative schedule. CoreInk owns clock validation,
  Hebrew calendar calculation, zmanim, settings, user interface, and the
  authoritative desired relay state. S31 applies and caches CoreInk-generated
  schedules.
- Profile B: S31-local brain. CoreInk owns trusted time, setup, settings
  distribution, and status display. Each S31 keeps the full calendar/zmanim
  engine, calculates its own policy, and reports its calculated state back to
  CoreInk.

Profile A remains the centralized-controller target. Profile B is the
maintainer-first alternative that may be lower risk for a first version because
it reuses the existing S31 calendar engine.

## Locked Rules

- Schedule packets or settings snapshots fully replace older data. They are
  never patched.
- `schedule_generation` or `settings_generation` only increases within a
  `controller_epoch` and survives CoreInk reboots.
- All relay instructions are idempotent `set state` operations. Never use
  `toggle`.
- S31 acknowledges schedule acceptance separately from actual relay
  application.
- CRC is mandatory for corruption detection.
- HMAC with a pre-shared key is mandatory for product firmware. It may be
  disabled only in explicit lab/debug builds.
- S31 executes cached transitions or locally calculated calendar policy only
  while its time and schedule/settings are valid.
- Any uncertainty falls into an explicitly configured fail-safe state.
- Public or local NTP is optional convenience only. Fully offline operation must
  not depend on WAN internet, a router, DNS, mDNS, or a home LAN.
- CoreInk owns the private controller network for the V1 product profile.
- V1 strictly uses the CoreInk onboard BM8563 RTC. External RTC modules are out
  of scope for V1 product firmware and hardware.

## Device Roles

### CoreInk Controller

Shared responsibilities:

- Maintain the authoritative clock state.
- Validate onboard RTC time before using it.
- Accept manual time correction through the local UI.
- Use public or local NTP only when available and trusted.
- Serve as the local time source for S31 devices.
- Provide the private control link when no home LAN exists.
- Persist `controller_epoch` and generation counters across reboot.
- Track S31 acknowledgements, relay output state reports, firmware versions, and
  fail-safe reasons.

Profile A responsibilities:

- Compute Hebrew calendar state and zmanim.
- Produce complete schedule packets for each S31.
- Stop issuing authoritative schedules when its own time is invalid.

Profile B responsibilities:

- Produce complete settings snapshots for each S31.
- Distribute authenticated time updates.
- Optionally run the same calendar engine as a reference calculation.
- Compare S31-reported calculated policy and next transition.
- Display disagreement if S31 devices diverge from each other or from CoreInk's
  reference calculation.

### S31 Satellite

Shared responsibilities:

- Apply relay state from accepted policy.
- Lock or unlock the physical button from accepted policy.
- Reject stale, corrupt, unauthenticated, or invalid controller data.
- Execute relay policy only while local time is valid.
- Report relay output state, button lock state, current
  generation, boot id, time validity, and fail-safe reason.
- Enter the configured fail-safe state when command, schedule, or time
  certainty is lost.

Profile A responsibilities:

- Cache the complete currently accepted schedule.
- Execute cached schedule transitions while local time and schedule validity
  remain acceptable.

Profile B responsibilities:

- Cache the complete currently accepted settings snapshot.
- Run the full Hebrew calendar and zmanim engine locally.
- Calculate current policy and upcoming transitions on-device.
- Report the calculated current policy, next transition, reason,
  `settings_generation`, and calendar-engine version to CoreInk.

## Selected Hardware

M5Stack order, price snapshot as of 2026-07-17:

| Item | SKU | Role | Price |
| --- | --- | --- | ---: |
| CoreInk | K048 | Brain, display, controls, Wi-Fi, built-in battery | $29.90 |

M5Stack subtotal before shipping: $29.90.

Also required outside the M5Stack order:

- Short USB-C power cable.
- Certified grounded wall tap with an always-powered USB port.
- Mounting bracket or high-bond removable tape.
- Sonoff S31 relay plug.

## Time Architecture

The CoreInk is the local authority for S31 time in offline homes. The S31 must
not require public NTP, a router, or a home LAN.

CoreInk time sources, strongest first when available:

1. Trusted public NTP, when internet is available.
2. Trusted local NTP, when an offline LAN provides it.
3. Manual time entered directly on CoreInk.
4. Onboard BM8563 RTC during offline operation.

CoreInk must classify time before producing schedules:

```text
SYNCED
RTC_HOLDOVER
TIME_INVALID
```

`SYNCED` means CoreInk has recently confirmed time from trusted public NTP,
trusted local NTP, or deliberate manual setup.

`RTC_HOLDOVER` means no current NTP or manual source is active, but the onboard
BM8563 is valid and continuing from a previously trusted time. Holdover is valid
only if the RTC reports no low-voltage/lost-clock condition, the date is
plausible, and the configured maximum holdover window has not been exceeded.

`TIME_INVALID` means CoreInk cannot prove the current time. In this state it
must not create or replace schedules. It should surface the fault on the display
and local web UI.

Because V1 has no external RTC module, a fully depleted CoreInk battery can lose
the BM8563 clock. That is a designed failure mode: CoreInk time becomes invalid
until manual correction, trusted NTP, or another configured trusted time source
restores it.

## Clock Uncertainty Policy

Clock uncertainty is a product safety setting, not a halachic offset.

V1 policy fields:

```text
warn_clock_uncertainty_seconds
max_clock_uncertainty_seconds
rtc_drift_ppm_assumption
last_trusted_sync_utc
onboard_rtc_voltage_low
last_rtc_plausibility_check_utc
```

Initial engineering placeholders, not final product policy:

```text
warn_clock_uncertainty_seconds: 60
max_clock_uncertainty_seconds: 120
```

Estimated uncertainty should include configured RTC drift assumptions, time
since the last trusted synchronization or manual verification, RTC validity
flags, and any detected backward or implausible RTC movement.

If estimated uncertainty exceeds `warn_clock_uncertainty_seconds`, CoreInk shows
a service warning. If it exceeds `max_clock_uncertainty_seconds`, CoreInk enters
`TIME_INVALID`, stops generating new calendar-derived schedules, and lets S31
devices continue only according to their already accepted schedule and fail-safe
policy.

## V2 External RTC Extension

V1 does not include an external RTC. This section is reserved for a later V2
profile if the M5Stack Unit RTC HYM8563 is added.

V2 may add these time-health conditions:

```text
EXTERNAL_RTC_MISSING
RTC_DEGRADED
RTC_DISAGREEMENT
```

V2 time-source order may become:

1. Trusted public NTP, when internet is available.
2. Trusted local NTP, when an offline LAN provides it.
3. Manual time entered directly on CoreInk.
4. External RTC Unit during offline operation.
5. Onboard BM8563 RTC as wake source and secondary diagnostic.

V2 rules:

- External RTC retention does not imply long-term accuracy.
- External RTC VL/lost-power is validity information, not battery capacity.
- External RTC cannot wake CoreInk through the Grove connector.
- Onboard RTC or ESP32 timer remains the wake source.
- Both RTC components must use `update_interval: never` and be orchestrated by
  one clock manager.
- If external RTC is configured as installed hardware and goes missing, CoreInk
  enters `RTC_DEGRADED` or `TIME_INVALID` according to policy rather than
  silently downgrading forever.
- External RTC values, like all persisted time, are stored and compared
  internally as UTC.

## V1 Product Scope

The first product firmware should intentionally support a narrower matrix than
the full architecture:

- One CoreInk controller.
- One to four S31 satellites.
- CoreInk onboard BM8563 RTC only; no external RTC module.
- CoreInk-owned SoftAP/HTTP baseline.
- ESP-NOW alternative for S31 control only after pairing, channel, ACK/retry,
  payload-size, and RF tests pass.
- HTTP with canonical JSON payloads for baseline implementation.
- Manual time setup plus optional trusted NTP.
- Fixed, selectable fail-safe policies.
- No cloud features.
- Home-LAN mode reserved for V2.

The protocol leaves room for more S31s and V2 home-LAN operation, but V1
testing and support should not depend on those paths.

## Identity And Generations

Controller identity has two parts:

```text
controller_id
controller_epoch
```

`controller_id` identifies the package or controller. `controller_epoch`
identifies the current pairing lifetime for that controller. A replacement
CoreInk, factory reset, or deliberate re-pair creates a new `controller_epoch`
and protocol key.

`controller_epoch` must be generated from at least 128 bits of cryptographic
random data and encoded as a fixed-length base64url or hex string. It must not
be derived from a timestamp, MAC address, counter, or short random suffix.

Profile A schedules use:

```text
schedule_generation
```

`schedule_generation` is monotonic only within the current `controller_epoch`.
Use an unsigned 64-bit integer for product firmware. If it ever reaches its
maximum value, CoreInk must refuse to generate more schedules until a service
flow creates a new `controller_epoch` and re-pairs the S31 devices.

Profile B settings snapshots use:

```text
settings_generation
```

`settings_generation` follows the same rules: monotonic only within
`controller_epoch`, persisted by CoreInk, rejected when lower than the cached
value, accepted idempotently when the same signed payload is received again, and
rejected if the same generation arrives with different contents.

Factory reset and replacement rules:

- If CoreInk flash is erased without a backup restore, it creates a new
  `controller_epoch` and key. Existing S31s reject it until re-paired.
- If CoreInk is restored from a valid backup, it may keep the previous
  `controller_epoch`, key, and last generation counters.
- A replacement CoreInk must use a service pairing flow that writes the new
  controller identity and key to each S31.
- An S31 moved between packages must be factory-reset or re-paired. Re-pairing
  wipes cached schedules/settings, persisted generation state, and protocol keys
  for the prior package.
- An S31 must reject schedules from an unexpected `controller_id` or
  `controller_epoch`.

## Offline Network Topology

The no-LAN topology is controller-owned Wi-Fi:

```text
CoreInk SoftAP: Melacha Controller
CoreInk AP IP: 192.168.4.1
S31 mode: Wi-Fi station joined to the CoreInk SoftAP
Schedule direction: CoreInk pushes schedules to registered S31 addresses
Time direction: S31 obtains time from CoreInk
Status direction: CoreInk polls S31 or S31 posts status to CoreInk
```

This avoids dependency on a router, router DHCP, DNS, mDNS, or WAN. CoreInk's
SoftAP DHCP may assign S31 addresses, or the S31 devices may use preconfigured
fixed addresses. The S31 does not need discovery for the controller because the
controller AP address is fixed.

CoreInk must not rely only on a hard-coded S31 IP table. Each S31 registers
after joining the AP, and CoreInk maintains a device-ID-to-IP table from
authenticated registration and status messages. DHCP reservations by MAC are
allowed as an optimization, but registration is the source of truth for the
current contact address.

V2 Home-LAN mode may be supported after V1:

```text
CoreInk and S31 join the same router Wi-Fi
S31 uses configured CoreInk IP/hostname
WAN NTP remains optional
```

The protocol must work in no-LAN mode first.

Operational consequence: a CoreInk that must keep a Wi-Fi SoftAP available for
S31 control should be treated as externally powered. The 390 mAh battery is
useful for RTC/display holdover, but it is not a practical always-on Wi-Fi power
source for a Shabbos/Yom Tov control window.

## Transport Pattern

The baseline V1 transport uses direct HTTP over the CoreInk-owned Wi-Fi link.
Profile A is push-first for schedules, after S31-initiated registration tells
CoreInk which address currently belongs to each device ID. Profile B can use the
same baseline for settings snapshots and policy reports.

CoreInk to S31:

```text
POST http://{s31_ip}/api/v1/schedule
POST http://{s31_ip}/api/v1/settings
GET  http://{s31_ip}/api/v1/status
```

S31 to CoreInk:

```text
POST http://192.168.4.1/api/v1/register
POST http://192.168.4.1/api/v1/time
POST http://192.168.4.1/api/v1/ack
POST http://192.168.4.1/api/v1/status
POST http://192.168.4.1/api/v1/policy_report
```

ACKs may also be returned directly in the HTTP response to
`POST /api/v1/schedule`, but durable state reporting must still be available
after reconnect.

In Profile A, CoreInk pushes a full schedule on boot, after S31 registration,
after settings changes, after reconnection, and before cached schedules approach
expiry. In Profile B, CoreInk pushes authenticated time and full settings
snapshots, then waits for S31 policy reports. It also reconciles by polling or
receiving status. Failed requests retry with bounded backoff.

An optional S31 pull endpoint can be added later as a recovery mechanism, but it
is not required for the first offline appliance profile.

## ESP-NOW Transport Alternative

ESP-NOW may be used as an alternative transport for CoreInk/S31 control data.
It changes the transport, not the safety rules. Authentication, generations,
idempotent set-state behavior, stale rejection, ACKs, and fail-safe behavior
remain mandatory.

ESP-NOW properties that affect this protocol:

- It is connectionless peer-to-peer Wi-Fi messaging. It does not need an access
  point, DHCP, IP addressing, DNS, mDNS, or HTTP.
- It is not a mesh protocol. It does not provide routing or forwarding by
  itself. Any repeater behavior must be a separate application protocol or a
  different mesh framework.
- MAC-layer send success does not prove the receiving application accepted or
  processed the message. Application-level ACKs, sequence numbers, retries, and
  duplicate suppression are still required.
- Devices must be on the correct Wi-Fi channel. If CoreInk also runs SoftAP or
  AP+STA, channel behavior must be fixed and tested.
- Encrypted unicast can be used with paired-device keys, but product firmware
  must still keep application-level HMAC for authenticated time/settings/policy.
- Broadcast discovery must not carry secrets or trusted control data.
- Because S31 is ESP8266-class hardware, use a conservative 250-byte ESP-NOW
  payload budget unless the exact firmware stack proves a larger interoperable
  limit. Larger data needs explicit fragmentation and reassembly.

Recommended ESP-NOW message classes:

```text
HELLO
TIME_SYNC_REQUEST
TIME_SYNC_RESPONSE
SETTINGS_SNAPSHOT_CHUNK
SETTINGS_ACK
POLICY_REPORT
STATUS_REPORT
FAILSAFE_REPORT
```

Profile fit:

- Profile A can run over ESP-NOW, but full rolling schedules are likely too
  large for simple single-frame messages. Use HTTP/JSON first, or define
  compact binary/CBOR framing plus fragmentation.
- Profile B fits ESP-NOW better because CoreInk sends compact time/settings
  updates and S31 calculates the schedule locally.

ESP-NOW frame rules:

- Every trusted frame includes `protocol_version`, `controller_id`,
  `controller_epoch`, source device id, destination device id or broadcast class,
  message type, monotonically increasing sequence number, and HMAC.
- Frames are idempotent. Retransmission must not change relay state except by
  reapplying the same desired policy or replacing settings with the same
  generation.
- Settings snapshots are still full replacements. If split into chunks, the S31
  must not activate the new settings until all chunks pass authentication,
  length, CRC, generation, and structure checks.
- Time responses include a nonce from the request and are not trusted without a
  valid HMAC.

## HTTP And Parser Limits

V1 firmware must reject oversized or overly complex payloads before parsing them
deeply.

Initial limits:

```text
max_schedule_body_bytes: 8192
max_status_body_bytes: 2048
max_ack_body_bytes: 2048
max_time_body_bytes: 1024
max_json_depth: 6
max_transitions_per_schedule: 16
max_id_length: 64
max_reason_length: 32
max_error_string_length: 64
```

V1 rejects unknown top-level fields, duplicate JSON object keys, non-integer
timestamps, floating-point values, and arrays longer than their schema limit.
This keeps malformed packets from exhausting ESP8266 memory or creating
ambiguous signed payloads.

## S31 Local Time Source

The S31 needs valid local time to execute cached UTC transitions. In an offline
home, its local time source is the CoreInk over the controller-owned Wi-Fi link.

Allowed implementations:

- CoreInk provides a local SNTP/NTP service, and S31 points its SNTP client at
  the fixed CoreInk AP IP or configured CoreInk address.
- CoreInk provides a time-sync HTTP endpoint, and S31 uses a firmware time
  component or custom setter to update its local epoch.

V1 should use an authenticated HTTP time endpoint if practical. If local SNTP is
used, treat it only as rough local time; authenticated schedule/settings
validation still decides what may be executed. The protocol requirement is that
S31 time can be restored from CoreInk without WAN internet and without trusting
unauthenticated relay-control data.

S31 time is valid only when:

- The last time sync came from the paired CoreInk or another explicitly
  configured local source.
- The received time passed plausibility checks.
- The last sync age is within the configured maximum holdover window.
- The S31 has not rebooted into an unknown epoch since that sync.

If S31 reboots and cannot restore valid time, cached UTC schedules are not safe
to execute, and local calendar calculations are not safe to apply. It must enter
fail-safe until time returns and the cached schedule/settings are fresh enough to
use.

## Canonical Payloads

CRC and HMAC are computed over defined bytes, not over a vague JSON object.

V1 uses canonical JSON:

- UTF-8 encoding.
- Object keys sorted lexicographically by byte value.
- No insignificant whitespace.
- Arrays preserve their semantic order.
- Unix timestamps are integers.
- Enums are uppercase strings unless otherwise specified.
- Booleans and null use JSON lowercase literals.
- Floating-point values are not used in schedule packets.

The signed payload is the schedule object without the `crc32` and `hmac`
members. Verification parses the received JSON, removes `crc32` and `hmac`,
re-serializes the signed payload using the canonical rules, and then verifies:

```text
crc32 = CRC-32C(canonical_signed_payload)
hmac  = HMAC-SHA256(canonical_signed_payload, protocol_secret)
```

`crc32` is encoded as 8 lowercase hex characters. `hmac` is encoded as 64
lowercase hex characters. CoreInk should transmit the whole JSON body in
canonical form as well, so logs, debug tools, and device calculations all see
the same bytes.

## Schedule Packet

Profile A uses schedule packets.

The schedule packet is the authoritative state sent from CoreInk to S31. A new
accepted packet fully replaces the prior accepted schedule.

Suggested canonical shape:

```json
{
  "protocol_version": 1,
  "controller_id": "coreink-001",
  "controller_epoch": "Qmb2Tj6pRkq3x9sW5vL0Yg",
  "target_device_id": "s31-kitchen",
  "schedule_generation": 1842,
  "schedule_id": "1842-7d4a9c21",
  "created_at_utc": 1784238600,
  "valid_from_utc": 1784238600,
  "refresh_required_by_utc": 1784325000,
  "valid_until_utc": 1784411400,
  "transition_horizon_until_utc": 1785456000,
  "time_confidence": {
    "state": "SYNCED",
    "last_sync_utc": 1784238500,
    "uncertainty_seconds": 2
  },
  "time_sync_required_by_utc": 1784325000,
  "transitions": [
    {
      "transition_id": "1842-0001",
      "at_utc": 1784303400,
      "desired_relay_state": "OFF",
      "physical_button_locked": true,
      "protected_led": true,
      "reason": "shabbos_start"
    },
    {
      "transition_id": "1842-0002",
      "at_utc": 1784392200,
      "desired_relay_state": "ON",
      "physical_button_locked": false,
      "protected_led": false,
      "reason": "shabbos_end"
    }
  ],
  "current_policy": {
    "desired_relay_state": "ON",
    "physical_button_locked": false,
    "protected_led": false,
    "effective_at_utc": 1784238600,
    "reason": "weekday"
  },
  "failsafe_policy": {
    "execute_cached_schedule": true,
    "after_expiry": "PROTECTED_STATE",
    "desired_relay_state": "OFF",
    "physical_button_locked": true,
    "protected_led": true,
    "led_pattern": "shabbos_failsafe"
  },
  "crc32": "7d4a9c21",
  "hmac": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
}
```

Notes:

- `controller_epoch` defines the current pairing lifetime.
- `schedule_generation` is monotonic within `controller_epoch`, not per S31.
- `created_at_utc` is when CoreInk created the schedule.
- `refresh_required_by_utc` is when CoreInk expects to refresh the schedule.
- `valid_until_utc` is the hard limit for autonomous execution unless an
  explicit grace policy says otherwise.
- `transition_horizon_until_utc` is the last future transition included in the
  cache, which may be much farther out than `valid_until_utc`.
- `time_confidence` tells the S31 and UI how trusted the source clock was when
  CoreInk generated the schedule.
- `time_sync_required_by_utc` lets CoreInk force a time refresh before the
  schedule itself expires.
- `current_policy` is an idempotent immediate `set state` target.
- `transitions` are future idempotent `set state` targets.
- `failsafe_policy` is explicit in every schedule so the S31 can apply the
  correct fail-safe behavior even when CoreInk disappears.

## Profile B Settings Snapshot

Profile B uses settings snapshots instead of controller-generated schedules.
CoreInk sends the complete configuration required for every S31 to calculate its
own calendar policy.

A new accepted settings snapshot fully replaces the prior accepted settings. Do
not patch individual fields.

Required settings snapshot content:

```text
protocol_version
controller_id
controller_epoch
target_device_id
settings_generation
created_at_utc
valid_from_utc
refresh_required_by_utc
valid_until_utc
calendar_engine_min_version
timezone_id
dst_rule_or_tz_version
latitude_e7
longitude_e7
eretzyisrael_mode
normal_or_inverse_mode
early_shabbos_policy
candle_lighting_offset
havdalah_method
zmanim_mode
degree_mode_values
minutes_mode_values
per_plug_behavior
button_lock_policy
failsafe_policy
clock_uncertainty_policy
crc32
hmac
```

Profile B behavior:

- S31 may store a newer authenticated settings snapshot while its local time is
  invalid.
- S31 must not execute calendar-derived relay policy until both time and
  settings freshness are valid.
- After accepting settings, S31 calculates its own current policy and upcoming
  transitions.
- S31 reports its calculated current policy, next transition, reason, and
  calendar-engine version to CoreInk.
- CoreInk displays disagreement if S31 reports differ from each other or from a
  CoreInk reference calculation.
- CoreInk updates settings by incrementing `settings_generation` and sending a
  complete replacement snapshot.
- CoreInk corrects time separately from settings, using authenticated time
  responses.

Profile B policy report fields:

```text
protocol_version
controller_id
controller_epoch
s31_id
boot_id
settings_generation
calendar_engine_version
calculation_fingerprint
time_valid
time_uncertainty_seconds
current_policy
next_transition
failsafe_active
failsafe_reason
hmac
```

`calculation_fingerprint` should be a compact digest over the inputs and
computed boundary results, not over arbitrary logs. It exists so CoreInk can
identify mismatched S31 calculations without needing to parse a large schedule.

Profile B disagreement handling:

- A disagreement is a visible diagnostic, not an automatic toggle.
- CoreInk may push the latest settings snapshot again.
- CoreInk may request a detailed calculation report from the disagreeing S31.
- If the disagreement affects current protected-period state and cannot be
  resolved, the affected S31 enters its configured fail-safe policy.

## Schedule Lifetime

The S31 may store a long transition horizon without treating that whole horizon
as fully authoritative during controller absence.

V1 defaults:

```text
transition_horizon: 14 days or at least 8 transitions, whichever covers more
refresh_required_by_utc: about 24 hours after schedule creation
valid_until_utc: about 48 hours after schedule creation
```

Behavior:

- Before `refresh_required_by_utc`, the schedule is fresh.
- After `refresh_required_by_utc`, the S31 may continue executing the schedule,
  but CoreInk should show a stale-refresh warning and aggressively retry.
- After `valid_until_utc`, the S31 enters the configured fail-safe state unless
  an explicit product policy allows grace execution toward
  `transition_horizon_until_utc`.
- Any grace policy must be visible in the schedule and in the CoreInk UI.

`current_policy` expires at `valid_until_utc` along with the rest of the
schedule. After that time it is no longer authoritative.

This avoids letting old settings remain silently authoritative for the full
cached horizon after a controller failure.

## Missed Transitions

When S31 regains valid time after an outage, it must not replay every missed
transition. It computes the effective policy at the current UTC time:

1. Start from `current_policy`.
2. Find the latest transition with `at_utc <= now_utc`.
3. Apply only that resulting policy.
4. ACK the applied effective policy and report the latest transition id used.

If `now_utc > valid_until_utc`, the S31 must not apply the schedule-derived
policy. It enters fail-safe unless an explicit grace policy allows continued
cached execution.

## Acknowledgements

Schedule/settings acceptance and physical application are separate
acknowledgements.
Every registration, ACK, and status payload must include `controller_id`,
`controller_epoch`, the S31 device id, and `boot_id` when a boot id exists.

### Acceptance ACK

Sent after the S31 validates and stores a Profile A schedule or a Profile B
settings snapshot.

```json
{
  "protocol_version": 1,
  "s31_id": "s31-kitchen",
  "controller_id": "coreink-001",
  "boot_id": "s31-boot-0098",
  "ack_type": "SCHEDULE",
  "accepted": true,
  "controller_epoch": "Qmb2Tj6pRkq3x9sW5vL0Yg",
  "schedule_generation": 1842,
  "settings_generation": null,
  "accepted_at_utc": 1784238602,
  "accepted_at_uptime_ms": 421000,
  "cached_transition_count": 2,
  "time_valid": true,
  "execution_state": "READY_TO_EXECUTE",
  "reject_reason": null,
  "hmac": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
}
```

Valid rejection reasons:

```text
bad_crc
bad_hmac
wrong_controller
wrong_target_device
older_generation
duplicate_generation_different_payload
created_in_future
expired_schedule
invalid_transition_order
unsupported_protocol_version
storage_failed
calendar_engine_unsupported
```

Duplicate same-`schedule_generation` same-payload schedules are accepted
idempotently and reported as already cached.

On `storage_failed`, the S31 must preserve the previous valid schedule/settings
if possible. If no previous valid schedule/settings exists, it enters fail-safe.

If S31 time is invalid, the S31 may still authenticate, validate structure, and
store a newer schedule. The ACK must make the execution state explicit:

```text
READY_TO_EXECUTE
STORED_PENDING_TIME
REJECTED
```

`STORED_PENDING_TIME` means the schedule passed authentication, target, epoch,
`schedule_generation`, CRC, HMAC, and structural checks, but freshness checks
that require local time are deferred. In that state the S31 must not apply
`current_policy` or timestamped transitions. After time becomes valid, it
re-validates `created_at_utc`, `valid_from_utc`, `refresh_required_by_utc`, and
`valid_until_utc` before execution.

When S31 time is invalid, UTC acknowledgement timestamps must be omitted or set
to null, and the S31 must include an uptime-based timestamp instead.

### Application ACK

Sent whenever the S31 applies or confirms a relay/button state.

```json
{
  "protocol_version": 1,
  "s31_id": "s31-kitchen",
  "controller_id": "coreink-001",
  "boot_id": "s31-boot-0098",
  "controller_epoch": "Qmb2Tj6pRkq3x9sW5vL0Yg",
  "schedule_generation": 1842,
  "transition_id": "1842-0001",
  "applied_at_utc": 1784303401,
  "desired_relay_state": "OFF",
  "relay_output_state": "OFF",
  "physical_button_locked": true,
  "protected_led": true,
  "relay_output_confirmed": true,
  "physical_contact_verified": false,
  "success": true,
  "failure_reason": null,
  "hmac": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
}
```

Application failure reasons:

```text
relay_apply_failed
button_lock_failed
time_invalid
schedule_invalid
transition_expired
entered_failsafe
```

`relay_output_confirmed` means the S31 firmware set the relay GPIO/output to the
requested state. It does not prove the mechanical relay contacts physically
closed unless separate contact feedback hardware exists.

## Status Report

The S31 should expose or send a compact status report on boot, periodically, and
after every schedule or relay event. In no-LAN mode, CoreInk can poll this from
the S31 address, and the S31 can also post it to the CoreInk AP address after
important events.

```json
{
  "protocol_version": 1,
  "s31_id": "s31-kitchen",
  "controller_id": "coreink-001",
  "boot_id": "s31-boot-0098",
  "uptime_seconds": 421,
  "time_valid": true,
  "last_time_sync_utc": 1784238590,
  "last_controller_seen_utc": 1784238602,
  "controller_epoch": "Qmb2Tj6pRkq3x9sW5vL0Yg",
  "cached_schedule_generation": 1842,
  "cached_settings_generation": null,
  "calendar_engine_version": null,
  "schedule_valid": true,
  "settings_valid": null,
  "schedule_valid_until_utc": 1784411400,
  "settings_valid_until_utc": null,
  "transition_horizon_until_utc": 1785456000,
  "requested_relay_state": "ON",
  "relay_output_state": "ON",
  "physical_button_locked": false,
  "protected_led": false,
  "relay_output_confirmed": true,
  "physical_contact_verified": false,
  "failsafe_active": false,
  "failsafe_reason": null,
  "hmac": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
}
```

## Profile A Stale Schedule Rules

S31 must reject a schedule when:

- `protocol_version` is unsupported.
- `controller_id` does not match the paired controller.
- `controller_epoch` does not match the paired epoch.
- `target_device_id` does not match the S31.
- CRC fails.
- HMAC is required and fails.
- `schedule_generation` is lower than the cached `schedule_generation` for that
  epoch.
- `schedule_generation` equals the cached `schedule_generation` but the payload
  differs.
- S31 time is valid and `created_at_utc` is implausibly far in the future.
- S31 time is valid and `valid_until_utc` is already expired.
- Transition timestamps are unordered.
- Any transition is outside the schedule validity window.

S31 may accept a higher-`schedule_generation` schedule while currently in
fail-safe, but it may not execute time-based transitions until local time is
valid.

## Profile B Stale Settings Rules

S31 must reject a settings snapshot when:

- `protocol_version` is unsupported.
- `controller_id` does not match the paired controller.
- `controller_epoch` does not match the paired epoch.
- `target_device_id` does not match the S31.
- CRC fails.
- HMAC is required and fails.
- `settings_generation` is lower than the cached `settings_generation` for that
  epoch.
- `settings_generation` equals the cached `settings_generation` but the payload
  differs.
- S31 time is valid and `created_at_utc` is implausibly far in the future.
- S31 time is valid and `valid_until_utc` is already expired.
- Required calendar/zmanim/timezone fields are missing or out of range.
- `calendar_engine_min_version` is newer than the S31 firmware supports.

S31 may accept and store a higher-`settings_generation` snapshot while currently
in fail-safe or while local time is invalid, but it may not execute
calendar-derived relay policy until local time and settings freshness are valid.

## Fail-Safe Behavior

Fail-safe is a configured state, not an implicit firmware accident.

Minimum fail-safe fields:

```text
desired_relay_state
physical_button_locked
protected_led
led_pattern
reason
entered_at_utc_or_uptime
```

S31 enters fail-safe when:

- It boots without valid time and without an immediately usable controller.
- The cached schedule is missing, corrupt, expired, or unauthenticated.
- The cached schedule is valid but S31 time is invalid.
- The controller has been absent beyond the configured communication grace
  period and the schedule is not valid far enough forward.
- A required transition cannot be applied.
- A command or schedule is ambiguous, stale, or conflicting.

When S31 enters fail-safe:

- Apply the configured fail-safe relay state.
- Apply the configured fail-safe button lock state.
- Apply the configured fail-safe LED state.
- Persist the fail-safe reason if storage budget allows.
- Report the reason on the next successful controller contact.

## Boot And Button Safety

Physical button lockout is firmware-enforced. The product must still be safe
before normal firmware services are fully running.

S31 boot requirements:

- Before application firmware starts, the hardware-safe relay expectation is
  `OFF`; firmware policy is not active yet.
- After application GPIO initialization, relay restore/default state is the
  configured fail-safe relay state.
- After application GPIO initialization, physical button handling starts disabled
  or locked.
- No short press may toggle the relay until the firmware has loaded policy,
  initialized lockout state, and decided button handling is allowed.
- Crash loops and watchdog resets return to the same fail-safe defaults.
- Brownout recovery returns to the same fail-safe defaults.
- The protected LED state must not be treated as proof that the relay or button
  state is safe; it is only an indicator.

Validation must include bootloader interval, normal reboot, watchdog reboot,
power interruption, brownout-like rapid cycling, and repeated Wi-Fi failure.

## Reconnection Flow

On S31 boot:

1. Generate a new `boot_id`.
2. Load cached schedule/settings, persisted `controller_epoch`, and persisted
   generation counters.
3. Join the configured CoreInk SoftAP. V2 may add a home-LAN network path.
4. Attempt time sync from CoreInk or configured local source.
5. In Profile A, if time is valid and cached schedule is valid, apply the
   correct current state from the cached schedule.
6. In Profile B, if time is valid and cached settings are valid, calculate and
   apply the correct current state locally.
7. If not valid, enter fail-safe.
8. Report status to CoreInk when the link is available.

On CoreInk seeing an S31:

1. Read S31 status.
2. Compare `cached_schedule_generation` or `cached_settings_generation`,
   `boot_id`, time validity, firmware/calendar-engine version, and relay output
   state.
3. In Profile A, if the S31 lacks the current schedule, push the full current
   schedule.
4. In Profile B, if the S31 lacks the current settings, push the full settings
   snapshot and request a policy report.
5. In Profile A, if the S31 has the current schedule but wrong relay output or
   button-lock state, push or request confirmation of the idempotent current
   policy.
6. In Profile B, if the S31 reports a calculated policy that disagrees with
   CoreInk or other S31s, surface disagreement and request a detailed report or
   re-push settings.
7. If CoreInk time is invalid, do not send trusted time, new schedules, or new
   settings snapshots. Surface the fault.

## Calendar Engine Boundary

The calendar/zmanim engine must output policy, not touch GPIO directly.

Required policy output:

```text
now_utc
time_state
is_melacha_restricted
desired_relay_state
physical_button_locked
protected_led_state
reason
current_boundary
next_transitions
failsafe_policy
```

Device-specific code then applies the policy:

- On legacy standalone S31 firmware, the policy can still drive local GPIO.
- In Profile A CoreInk controller firmware, the policy becomes a schedule packet
  for one or more S31 satellites.
- In Profile A S31 satellite firmware, no calendar calculation is required for
  normal operation; it consumes schedules and reports physical state.
- In Profile B S31 firmware, the same calendar engine runs locally and emits
  policy directly on the S31, while CoreInk optionally runs it as a reference
  calculation.

This is the dividing line for the refactor.
