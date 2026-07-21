# CoreInk And S31 State Machines

This document defines the firmware states that must be implemented before the
CoreInk/S31 split is coded. Product context lives in
`COREINK_OFFLINE_PRODUCT_PLAN.md`; packet rules live in
`COREINK_S31_PROTOCOL.md`; exact JSON schemas live in
`COREINK_S31_JSON_SCHEMAS.md`.

## Shared Definitions

Time states:

```text
SYNCED
RTC_HOLDOVER
TIME_INVALID
```

Schedule execution states:

```text
NO_SCHEDULE
STORED_PENDING_TIME
READY_TO_EXECUTE
STALE_REFRESH
EXPIRED
```

Relay safety states:

```text
NORMAL_POLICY
PROTECTED_FAILSAFE
APPLY_FAILED
```

An implementation may model these as separate flags instead of one large enum,
but the externally visible behavior must match these states.

## CoreInk State Machine

CoreInk has four coupled state regions: boot, time, network, and schedule.

### CoreInk Boot

| State | Entry action | Exit condition |
| --- | --- | --- |
| `BOOT_INIT` | Start display, load settings, load `controller_epoch`, load `schedule_generation`. | Settings storage read completed. |
| `RTC_VALIDATE` | Read onboard RTC, check voltage-low/lost-clock flags, and run plausibility checks. | Time classified as `SYNCED`, `RTC_HOLDOVER`, or `TIME_INVALID`. |
| `AP_START` | Start CoreInk SoftAP and local web UI. | AP is accepting clients. |
| `SERVICE_READY` | Start time endpoint, registration endpoint, schedule/status endpoints. | Normal runtime. |

If storage is corrupt, CoreInk enters `TIME_INVALID` and service mode. It must
not generate calendar-derived schedules from untrusted settings.

### CoreInk Time

| State | Entry action | Events |
| --- | --- | --- |
| `SYNCED` | Write trusted time to onboard RTC; record sync source and drift. | NTP/manual verification expires -> `RTC_HOLDOVER`; clock contradiction -> `TIME_INVALID`. |
| `RTC_HOLDOVER` | Use onboard RTC as current time; show holdover age and uncertainty. | Trusted NTP/manual correction -> `SYNCED`; uncertainty too high or RTC fault -> `TIME_INVALID`. |
| `TIME_INVALID` | Stop generating new schedules; show clock fault; allow manual correction. | Manual correction or trusted NTP -> `SYNCED`. |

Clock uncertainty hard rule:

```text
estimated_uncertainty_seconds > max_clock_uncertainty_seconds -> TIME_INVALID
```

The uncertainty estimate includes configured drift assumption, time since last
trusted verification, RTC validity flags, and impossible or backward RTC
movement.

### V2 External RTC Events

V1 uses only the onboard CoreInk RTC. If V2 adds the external RTC Unit, add these
events and states without changing the V1 path:

| Event/state | Meaning |
| --- | --- |
| `EXTERNAL_RTC_MISSING` | External RTC is configured but not detected. |
| `RTC_DEGRADED` | External RTC is unavailable or suspect, but a short configured grace path exists. |
| `RTC_DISAGREEMENT` | External and onboard RTC differ beyond threshold. |

V2 wake-source rule:

- Onboard RTC or ESP32 timer wakes CoreInk.
- External RTC supplies retained authoritative time after wake, if valid.
- External RTC cannot wake CoreInk directly through the Grove connector.
- If external RTC is required and unavailable beyond grace, CoreInk enters
  `TIME_INVALID`.

### CoreInk Network

| State | Entry action | Events |
| --- | --- | --- |
| `AP_DOWN` | No control network available. | Start AP -> `AP_UP`. |
| `AP_UP` | Accept S31 registration and admin web clients. | S31 registers -> update device-ID-to-IP table. |
| `S31_REGISTERED` | Mark S31 reachable, record IP, boot id, firmware version, cached schedule state. | Status timeout -> `S31_STALE`; new registration -> refresh table. |
| `S31_STALE` | Show stale device warning; keep retrying bounded backoff. | Registration/status returns -> `S31_REGISTERED`. |

Registration is the source of truth for current S31 address. Fixed IPs and DHCP
reservations are only optimizations.

### CoreInk Schedule

| State | Entry action | Events |
| --- | --- | --- |
| `NO_VALID_TIME` | Do not generate schedules. | Time becomes `SYNCED` or `RTC_HOLDOVER` -> `GENERATE_SCHEDULE`. |
| `GENERATE_SCHEDULE` | Compute policy and rolling transitions; increment `schedule_generation`. | Schedule serialized and signed -> `PUSH_PENDING`. |
| `PUSH_PENDING` | Push full schedule to each registered S31. | All ACK accepted -> `CONFIRMED`; some timeout -> `PARTIAL_CONFIRM`. |
| `CONFIRMED` | Display confirmed plug count and next transition. | Settings change, refresh deadline, or time-confidence change -> `GENERATE_SCHEDULE`. |
| `PARTIAL_CONFIRM` | Show which plugs are missing; retry with bounded backoff. | All ACK accepted -> `CONFIRMED`; schedule expires -> keep warning. |
| `TIME_INVALID_HOLD` | Stop schedule generation; keep current diagnostics visible. | Time fixed -> `GENERATE_SCHEDULE`. |

Any setting that affects time, zmanim, relay behavior, fail-safe policy, device
identity, or schedule lifetime must trigger `GENERATE_SCHEDULE`.

## S31 State Machine

The S31 starts in the safest state the hardware and firmware can provide. There
is an important boundary before application firmware is running: firmware policy
cannot lock GPIO0 or energize a relay before the ESP8266 has booted the app.

### S31 Boot

| State | Entry action | Exit condition |
| --- | --- | --- |
| `PRE_APPLICATION` | Hardware/bootloader interval before application policy is active. Relay expectation is hardware-safe `OFF`; GPIO0 boot strap behavior still exists; firmware button lock is not yet active. | Application firmware starts and initializes GPIO. |
| `APPLICATION_FAILSAFE` | Set relay to configured fail-safe state; lock button handling; set protected LED. | GPIO initialized and fail-safe outputs applied. |
| `LOAD_CACHE` | Load cached schedule, `controller_epoch`, `schedule_generation`, protocol key, and device id. | Cache classified as present/corrupt/missing. |
| `JOIN_AP` | Join configured CoreInk SoftAP. | Link up -> `REGISTER`; timeout -> `LINK_DOWN_FAILSAFE`. |
| `REGISTER` | POST authenticated registration/status to CoreInk. | Registration accepted -> `SYNC_TIME`; rejected -> `PAIRING_REQUIRED_FAILSAFE`. |

The short-press button path must remain disabled through all boot states until a
valid policy explicitly allows it.

A configured fail-safe `ON` state cannot be guaranteed during
`PRE_APPLICATION`. It becomes enforceable only after application GPIO
initialization. This is especially important for inverse mode.

GPIO0 remains an ESP8266 boot strap before firmware starts. Holding the S31
button during power-up can prevent normal application boot. Firmware lockout
does not prevent that denial-of-service path; child-resistant products need a
physical button cover, recessed mount, or enclosure.

### S31 Time And Schedule

| State | Entry action | Events |
| --- | --- | --- |
| `LINK_DOWN_FAILSAFE` | Keep fail-safe relay/button/LED; retry AP join. | AP joined -> `REGISTER`. |
| `SYNC_TIME` | Obtain time from CoreInk NTP or authenticated time endpoint. | Time valid -> `VALIDATE_CACHE`; time unavailable -> `TIME_INVALID_FAILSAFE`. |
| `TIME_INVALID_FAILSAFE` | Keep fail-safe; accept authenticated schedules only as pending. | Time valid -> `VALIDATE_CACHE`. |
| `VALIDATE_CACHE` | Check CRC, HMAC, epoch, `schedule_generation`, validity window, and transition order. | Valid schedule -> `READY_TO_EXECUTE`; pending schedule without time -> `STORED_PENDING_TIME`; invalid -> `NO_SCHEDULE_FAILSAFE`. |
| `STORED_PENDING_TIME` | Store authenticated newer schedule but do not apply `current_policy` or transitions. | Time valid and freshness checks pass -> `READY_TO_EXECUTE`; freshness fails -> `EXPIRED_FAILSAFE`. |
| `READY_TO_EXECUTE` | Apply latest effective policy for current UTC time and arm future transitions. | Refresh deadline passes -> `STALE_REFRESH`; hard expiry -> `EXPIRED_FAILSAFE`; transition due -> `APPLY_TRANSITION`. |
| `STALE_REFRESH` | Continue executing valid schedule; report stale refresh; retry controller contact. | New schedule accepted -> `READY_TO_EXECUTE`; hard expiry -> `EXPIRED_FAILSAFE`. |

Invalid local time never by itself forbids storing a newer authenticated
schedule. It forbids freshness-dependent execution.

When recovering after missed transitions, S31 applies only the latest effective
policy at current UTC time. It does not replay each missed transition.

### S31 Relay Execution

| State | Entry action | Events |
| --- | --- | --- |
| `APPLY_CURRENT_POLICY` | Set relay output, button lock, and LED to `current_policy`. | Output command succeeds -> ACK application; fails -> `APPLY_FAILED_FAILSAFE`. |
| `APPLY_TRANSITION` | Set relay output, button lock, and LED for due transition. | Output command succeeds -> ACK application; fails -> `APPLY_FAILED_FAILSAFE`. |
| `PROTECTED_FAILSAFE` | Set configured fail-safe relay, lock button, set protected LED. | Valid time and valid schedule received -> `READY_TO_EXECUTE`. |
| `EXPIRED_FAILSAFE` | Same as protected fail-safe, with reason `schedule_expired`. | New valid schedule and time -> `READY_TO_EXECUTE`. |
| `APPLY_FAILED_FAILSAFE` | Same as protected fail-safe, with reason `apply_failed`. | Manual service or successful controller recovery policy -> `READY_TO_EXECUTE`. |

Application ACK confirms firmware output state only. It does not prove
mechanical relay-contact state without separate feedback hardware.

## Required Fault Tests

Before release, run these tests on real hardware:

- CoreInk boots with valid onboard RTC.
- CoreInk boots with onboard RTC voltage-low/lost-clock flag.
- CoreInk boots with impossible or out-of-range onboard RTC date.
- CoreInk battery fully depletes and onboard RTC loses time.
- Manual time correction moves CoreInk from `TIME_INVALID` to `SYNCED`.
- S31 boots with no CoreInk AP reachable.
- S31 boots with CoreInk AP reachable but no valid time.
- S31 accepts and stores a newer authenticated schedule while time is invalid.
- S31 refuses to execute that pending schedule until time becomes valid.
- S31 rejects lower `schedule_generation`.
- S31 rejects same `schedule_generation` with different payload.
- S31 rejects wrong `controller_epoch`.
- S31 survives CoreInk reboot while schedule remains valid.
- S31 enters fail-safe after `valid_until_utc`.
- S31 treats `current_policy` as expired after `valid_until_utc`.
- S31 applies latest effective policy after missed transitions, not each missed
  transition in sequence.
- S31 `PRE_APPLICATION` behavior is measured separately from firmware behavior.
- S31 button-held boot is tested as a denial-of-service path through GPIO0 boot
  strap behavior.
- S31 button cannot toggle relay after application startup during boot, crash
  loop, Wi-Fi outage, or protected period.
- S31 relay is hardware-safe `OFF` before application startup and changes to
  configured fail-safe state once firmware initializes GPIO.
- S31 watchdog reset returns to fail-safe defaults and reports new `boot_id`.
- Brownout-like rapid cycling returns S31 to fail-safe defaults.
- Flash write failure during schedule storage returns an explicit rejection and
  preserves the previous valid schedule if possible.
- Flash write failure with no previous valid schedule enters fail-safe.
