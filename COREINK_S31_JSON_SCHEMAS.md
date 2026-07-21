# CoreInk/S31 JSON Schemas And Invariants

This document defines the V1 HTTP/JSON payload contract. Protocol behavior is
specified in `COREINK_S31_PROTOCOL.md`; state behavior is specified in
`COREINK_S31_STATE_MACHINES.md`.

## Encoding Rules

All authenticated payloads use canonical JSON:

- UTF-8.
- Object keys sorted lexicographically by byte value.
- No insignificant whitespace.
- No duplicate object keys.
- Arrays preserve semantic order.
- Unix timestamps are unsigned integer seconds.
- Uptime fields are unsigned integer milliseconds or seconds as named.
- No floating-point values.
- Unknown top-level fields are rejected in V1.

Authentication:

```text
crc32 = CRC-32C(canonical_payload_without_crc32_or_hmac)
hmac  = HMAC-SHA256(canonical_payload_without_crc32_or_hmac, protocol_secret)
```

Schedule packets include both `crc32` and `hmac`. Other authenticated payloads
include `hmac`; for those payloads, the canonical authenticated bytes are the
payload without `hmac`.

Encoding:

```text
crc32: 8 lowercase hex chars
hmac: 64 lowercase hex chars
controller_epoch: 22 char base64url, 128 random bits, no padding
boot_id: 22 char base64url, 128 random bits, no padding
```

## Shared Limits

| Limit | Value |
| --- | ---: |
| `max_schedule_body_bytes` | 8192 |
| `max_status_body_bytes` | 2048 |
| `max_ack_body_bytes` | 2048 |
| `max_time_body_bytes` | 1024 |
| `max_json_depth` | 6 |
| `max_transitions_per_schedule` | 16 |
| `max_id_length` | 64 |
| `max_reason_length` | 32 |
| `max_error_string_length` | 64 |

## Shared Types

| Name | Type | Constraint |
| --- | --- | --- |
| `protocol_version` | integer | exactly `1` |
| `controller_id` | string | 1 to 64 chars |
| `controller_epoch` | string | 22 char base64url random id |
| `device_id`, `s31_id`, `target_device_id` | string | 1 to 64 chars |
| `boot_id` | string | 22 char base64url random id |
| `schedule_generation` | integer | unsigned 64-bit |
| `utc` fields | integer | unsigned Unix epoch seconds |
| relay state | string | `ON` or `OFF` |
| booleans | boolean | JSON `true` or `false` |

## Schedule Packet

Endpoint:

```text
POST http://{s31_ip}/api/v1/schedule
```

Required fields:

| Field | Type | Notes |
| --- | --- | --- |
| `protocol_version` | integer | shared type |
| `controller_id` | string | paired controller |
| `controller_epoch` | string | paired epoch |
| `target_device_id` | string | receiving S31 |
| `schedule_generation` | integer | monotonic within epoch |
| `schedule_id` | string | stable id for logs/UI |
| `created_at_utc` | integer | CoreInk schedule creation time |
| `valid_from_utc` | integer | first valid execution time |
| `refresh_required_by_utc` | integer | refresh deadline |
| `valid_until_utc` | integer | hard autonomous execution expiry |
| `transition_horizon_until_utc` | integer | last transition horizon |
| `time_confidence` | object | see below |
| `time_sync_required_by_utc` | integer | latest acceptable S31 time refresh |
| `current_policy` | object | immediate set-state policy |
| `transitions` | array | 0 to 16 transition objects |
| `failsafe_policy` | object | explicit fail-safe |
| `crc32` | string | 8 lowercase hex |
| `hmac` | string | 64 lowercase hex |

`time_confidence`:

| Field | Type | Values |
| --- | --- | --- |
| `state` | string | `SYNCED`, `RTC_HOLDOVER`, `TIME_INVALID` |
| `last_sync_utc` | integer or null | null only when never trusted |
| `uncertainty_seconds` | integer | estimated uncertainty |

`current_policy`:

| Field | Type | Values |
| --- | --- | --- |
| `desired_relay_state` | string | `ON`, `OFF` |
| `physical_button_locked` | boolean | required |
| `protected_led` | boolean | required |
| `effective_at_utc` | integer | policy effective time |
| `reason` | string | max 32 chars |

Transition object:

| Field | Type | Values |
| --- | --- | --- |
| `transition_id` | string | max 64 chars |
| `at_utc` | integer | must be ordered ascending |
| `desired_relay_state` | string | `ON`, `OFF` |
| `physical_button_locked` | boolean | required |
| `protected_led` | boolean | required |
| `reason` | string | max 32 chars |

`failsafe_policy`:

| Field | Type | Values |
| --- | --- | --- |
| `execute_cached_schedule` | boolean | required |
| `after_expiry` | string | `PROTECTED_STATE`, `HOLD_LAST_OUTPUT`, `RELAY_OFF`, `RELAY_ON` |
| `desired_relay_state` | string | `ON`, `OFF` |
| `physical_button_locked` | boolean | required |
| `protected_led` | boolean | required |
| `led_pattern` | string | max 32 chars |

## Registration

Endpoint:

```text
POST http://192.168.4.1/api/v1/register
```

Required fields:

| Field | Type | Notes |
| --- | --- | --- |
| `protocol_version` | integer | shared type |
| `controller_id` | string | paired controller |
| `controller_epoch` | string | paired epoch |
| `s31_id` | string | registering S31 |
| `boot_id` | string | generated on each boot |
| `firmware_version` | string | max 64 chars |
| `ip_address` | string | S31 current address as observed/configured |
| `time_valid` | boolean | S31 time validity |
| `cached_schedule_generation` | integer or null | null if no schedule |
| `hmac` | string | 64 lowercase hex |

## Time Request And Response

Endpoint:

```text
POST http://192.168.4.1/api/v1/time
```

Request fields:

| Field | Type | Notes |
| --- | --- | --- |
| `protocol_version` | integer | shared type |
| `controller_id` | string | paired controller |
| `controller_epoch` | string | paired epoch |
| `s31_id` | string | requesting S31 |
| `boot_id` | string | current boot |
| `nonce` | string | 22 char base64url random id |
| `hmac` | string | 64 lowercase hex |

Response fields:

| Field | Type | Notes |
| --- | --- | --- |
| `protocol_version` | integer | shared type |
| `controller_id` | string | paired controller |
| `controller_epoch` | string | paired epoch |
| `s31_id` | string | target S31 |
| `nonce` | string | echoes request nonce |
| `time_state` | string | `SYNCED`, `RTC_HOLDOVER`, `TIME_INVALID` |
| `now_utc` | integer or null | null when `TIME_INVALID` |
| `valid_for_seconds` | integer | 0 when `TIME_INVALID` |
| `uncertainty_seconds` | integer | CoreInk estimate |
| `hmac` | string | 64 lowercase hex |

S31 must not treat unauthenticated time as sufficient for relay execution.

## Schedule ACK

Endpoint:

```text
POST http://192.168.4.1/api/v1/ack
```

Required fields:

| Field | Type | Values |
| --- | --- | --- |
| `protocol_version` | integer | shared type |
| `controller_id` | string | paired controller |
| `controller_epoch` | string | paired epoch |
| `s31_id` | string | sending S31 |
| `boot_id` | string | current boot |
| `ack_type` | string | `SCHEDULE` |
| `accepted` | boolean | required |
| `schedule_generation` | integer | schedule being ACKed |
| `accepted_at_utc` | integer or null | null if S31 time invalid |
| `accepted_at_uptime_ms` | integer | required |
| `cached_transition_count` | integer | 0 to 16 |
| `time_valid` | boolean | required |
| `execution_state` | string | `READY_TO_EXECUTE`, `STORED_PENDING_TIME`, `REJECTED` |
| `reject_reason` | string or null | null when accepted |
| `hmac` | string | 64 lowercase hex |

## Application ACK

Endpoint:

```text
POST http://192.168.4.1/api/v1/ack
```

Required fields:

| Field | Type | Values |
| --- | --- | --- |
| `protocol_version` | integer | shared type |
| `controller_id` | string | paired controller |
| `controller_epoch` | string | paired epoch |
| `s31_id` | string | sending S31 |
| `boot_id` | string | current boot |
| `ack_type` | string | `APPLICATION` |
| `schedule_generation` | integer | active schedule |
| `transition_id` | string or null | null for current policy |
| `applied_at_utc` | integer or null | null if S31 time invalid |
| `applied_at_uptime_ms` | integer | required |
| `desired_relay_state` | string | `ON`, `OFF` |
| `relay_output_state` | string | `ON`, `OFF` |
| `physical_button_locked` | boolean | required |
| `protected_led` | boolean | required |
| `relay_output_confirmed` | boolean | firmware output confirmation |
| `physical_contact_verified` | boolean | false unless feedback hardware exists |
| `success` | boolean | required |
| `failure_reason` | string or null | null on success |
| `hmac` | string | 64 lowercase hex |

## Status

Endpoint:

```text
POST http://192.168.4.1/api/v1/status
GET  http://{s31_ip}/api/v1/status
```

Required fields in a status body:

| Field | Type | Values |
| --- | --- | --- |
| `protocol_version` | integer | shared type |
| `controller_id` | string | paired controller |
| `controller_epoch` | string | paired epoch |
| `s31_id` | string | reporting S31 |
| `boot_id` | string | current boot |
| `uptime_seconds` | integer | required |
| `time_valid` | boolean | required |
| `last_time_sync_utc` | integer or null | null if never valid |
| `last_controller_seen_utc` | integer or null | null if S31 time invalid |
| `cached_schedule_generation` | integer or null | null if no schedule |
| `schedule_valid` | boolean | required |
| `schedule_valid_until_utc` | integer or null | null if no valid schedule |
| `transition_horizon_until_utc` | integer or null | null if no valid schedule |
| `requested_relay_state` | string | `ON`, `OFF` |
| `relay_output_state` | string | `ON`, `OFF` |
| `physical_button_locked` | boolean | required |
| `protected_led` | boolean | required |
| `relay_output_confirmed` | boolean | required |
| `physical_contact_verified` | boolean | false unless feedback hardware exists |
| `failsafe_active` | boolean | required |
| `failsafe_reason` | string or null | max 64 chars |
| `hmac` | string | 64 lowercase hex |

## V2 Extension Fields

V1 implementations reject unknown top-level fields. If V2 adds the external RTC
or home-LAN profiles, those profiles must version their schemas explicitly
rather than silently adding fields to V1 payloads.

Reserved V2 time-health conditions include:

```text
EXTERNAL_RTC_MISSING
RTC_DEGRADED
RTC_DISAGREEMENT
```

These are not valid V1 `time_confidence.state` values. In V1, the externally
visible time states remain only `SYNCED`, `RTC_HOLDOVER`, and `TIME_INVALID`.

## Cross-Firmware Invariants

Both CoreInk and S31 test suites must enforce these invariants:

| Invariant | Required behavior |
| --- | --- |
| No toggle | All relay commands are absolute set-state policies. |
| Epoch match | S31 rejects unexpected `controller_id` or `controller_epoch`. |
| Generation monotonicity | Lower `schedule_generation` is stale; same generation with different payload is rejected. |
| Duplicate idempotence | Same generation and same CRC/HMAC may be accepted repeatedly with no behavior change. |
| Invalid S31 time | S31 may store authenticated schedules as `STORED_PENDING_TIME`, but may not execute them. |
| Current policy expiry | `current_policy` is not authoritative after `valid_until_utc`. |
| Missed transitions | After reconnect/time recovery, S31 applies the latest effective policy at current UTC, not every missed transition. |
| Boot fail-safe | Before S31 application firmware, relay expectation is hardware-safe `OFF`; after app GPIO initialization, relay/button/LED enter configured fail-safe before network or schedule logic. |
| Button lock | Short press cannot toggle until firmware explicitly allows it. |
| HMAC | Product firmware rejects unsigned or bad-HMAC payloads. |
| Canonicalization | Signatures are computed over canonical JSON bytes only. |
| Parser limits | Oversized bodies, excessive depth, duplicate keys, and too many transitions are rejected. |
| Storage failure | Failed schedule storage preserves previous valid schedule if possible; otherwise fail-safe. |
| Relay proof | Firmware may report relay output confirmed, not mechanical contact verified. |
