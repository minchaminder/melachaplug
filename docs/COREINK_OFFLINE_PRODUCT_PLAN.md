# Complete Offline Melacha Plug Plan

This document describes the complete offline CoreInk plus Sonoff S31 product
architecture. Protocol-level details live in `COREINK_S31_PROTOCOL.md`; device
state machines live in `COREINK_S31_STATE_MACHINES.md`; exact JSON schemas live
in `COREINK_S31_JSON_SCHEMAS.md`.

## Product Package

The package includes:

1. M5Stack CoreInk.
2. Certified grounded wall tap with an always-powered USB port.
3. Pre-flashed Sonoff S31.
4. Short USB-C cable and mounting bracket or high-bond removable tape.

Physical arrangement:

```text
Wall outlet
    |
    +-- USB wall tap
          |
          +-- USB-C power --> CoreInk
          |
          +-- Grounded AC outlet --> Sonoff S31 --> Appliance
```

CoreInk is powered directly from the wall tap, not from the S31 switched output.
Turning off the S31 relay must not turn off the CoreInk.

For prototypes, industrial removable mounting tape is acceptable. A finished
product should use a plastic bracket or enclosure that secures the CoreInk, USB
cable, and wall-tap arrangement without blocking buttons, ventilation, labels,
or outlets.

The end user must not open the S31, tap its internal power supply, or route any
low-voltage wires from its mains enclosure. Factory provisioning may require
opening and serial-flashing the S31 while it is completely disconnected from
mains power, followed by proper reassembly and inspection.

## Firmware Roles

The clean architecture has two firmware roles.

### CoreInk Brain

The CoreInk owns:

- Authoritative time.
- Onboard RTC management.
- Hebrew calendar.
- Shabbos and Yom Tov detection.
- Zmanim calculations.
- Latitude, longitude, timezone, and daylight-saving behavior.
- Eretz Yisrael or diaspora mode.
- Early Shabbos settings.
- Zmanim adjustments.
- Normal and inverse Shabbos modes.
- Per-plug configuration.
- Schedule generation.
- Display and buttons.
- Local web GUI.
- Private Wi-Fi network.
- Local time service.
- Desired relay state.
- System diagnostics.
- Error and fail-safe decisions.

### S31 Relay Satellite

The S31 owns:

- Physical relay output.
- Physical button handling and lockout.
- LED indication.
- Cached future transitions.
- Local execution of downloaded schedules.
- Reporting firmware state.
- Reporting relay command state.
- Reporting whether the button is locked.
- Entering fail-safe behavior when time, schedule, or communication is invalid.

In the CoreInk-authoritative schedule profile, the S31 contains no Jewish
calendar or zmanim calculations. It does not independently decide when Shabbos
begins. CoreInk decides what should happen; S31 safely applies it.

## Architecture Profiles

The plan now tracks two viable control profiles. They share the same hardware,
identity, authentication, time-validation, fail-safe, and boot-safety rules.

### Profile A: CoreInk-Authoritative Schedule

This is the detailed architecture in the original CoreInk/S31 plan.

CoreInk:

- Owns Hebrew calendar and zmanim calculation.
- Generates complete rolling schedules.
- Sends desired relay/button/LED policy to each S31.
- Compares desired state with S31 reported output state.

S31:

- Applies CoreInk-provided schedules.
- Caches transitions.
- Executes cached transitions while time and schedule remain valid.
- Does not contain calendar/zmanim decision logic.

Profile A gives one central schedule authority and smaller S31 decision logic.
The cost is that porting the full calendar engine to CoreInk becomes required
before the architecture is complete.

### Profile B: S31-Local Brain

This is the maintainer-first alternative and may be lower risk for an initial
version because it keeps the existing Melacha Plug brain on every S31.

CoreInk:

- Maintains and distributes trusted time.
- Sends location, timezone, daylight-saving, mode, zmanim, fail-safe, and
  button-lock settings.
- Owns the setup UI and local status display.
- Tracks firmware versions and settings generations.
- Receives each S31's calculated current policy and next transition.
- Displays disagreements between plugs or between CoreInk's reference
  calculation and S31 reports.

S31:

- Keeps the full Hebrew calendar and zmanim engine.
- Calculates its own protected periods and relay schedule.
- Applies relay, button, and LED policy locally.
- Reports its calculated current state, next transition, reason, settings
  generation, and calendar-engine version.
- Continues independently after CoreInk disappears, as long as its time,
  settings, and uncertainty window remain valid.

Profile B gives better redundancy after initialization: if CoreInk later loses
power or reboots, each powered S31 can keep running from its own local calendar
logic. It also reuses code that already exists on the S31.

The tradeoff is stronger firmware-version coupling. Every S31 must carry the
same tested calendar engine and must receive the same settings generation.
CoreInk needs explicit disagreement detection because a calculation bug or
version mismatch can make one plug diverge from another.

Recommended Profile B startup flow:

1. S31 boots relay `OFF` and button locked.
2. S31 receives authenticated time and full settings from CoreInk.
3. S31 validates and stores the settings snapshot.
4. S31 calculates its own current policy and rolling local schedule.
5. S31 reports the calculated current policy and next transition to CoreInk.
6. CoreInk compares reports and displays disagreements.
7. Once initialized, S31 continues independently while time/settings remain
   valid.
8. CoreInk periodically corrects time and pushes newer settings generations.

Profile B still needs the same hard rules:

- Full settings snapshots replace older settings. Do not patch settings.
- `settings_generation` is monotonic within `controller_epoch`.
- Time updates and settings snapshots are authenticated.
- Commands are idempotent set-state or set-settings operations.
- Any uncertainty enters the configured fail-safe state.
- S31 must not execute calendar-derived relay policy unless its time and
  settings are valid.

### Profile Selection Guidance

For a first shippable prototype, Profile B is probably the lower-risk path if
the existing S31 calendar/zmanim code is already trusted and fits comfortably in
flash with the required communication layer.

Profile A remains the cleaner centralized-controller architecture if the product
goal is to move calendar authority out of every relay plug. It should stay in
the docs because it is still a defensible later refactor.

## Onboard RTC

V1 strictly uses the CoreInk onboard RTC. It does not include or support the
external M5Stack Unit RTC module.

The onboard BM8563:

- Shares the CoreInk power/battery design.
- Provides offline holdover while the CoreInk battery remains alive.
- Becomes invalid if it reports voltage-low/lost-clock or fails plausibility
  checks.
- Must be restored by manual time entry or trusted NTP after complete clock
  loss.

The accepted V1 failure mode is that a fully depleted CoreInk battery may lose
time. In that case the system enters `TIME_INVALID` until a trusted source
restores the clock.

V2 external RTC support can be reconsidered after V1, but it is not part of the
first implementation or test matrix.

### V2 External RTC Option

V2 may add the M5Stack Unit RTC HYM8563 as an external retained-time source.
That hardware is explicitly out of scope for V1.

Potential V2 M5Stack order:

| Item | SKU | Role |
| --- | --- | --- |
| CoreInk | K048 | Brain, display, controls, Wi-Fi, built-in battery |
| RTC Unit HYM8563 | U126 | Battery-backed retained-time source |

V2 physical arrangement:

```text
USB-C power --> CoreInk
                  |
                  +-- Grove cable --> Unit RTC HYM8563
```

V2 external RTC behavior:

- The external HYM8563 retains time on its own button battery while CoreInk is
  completely unpowered.
- It improves retention, not long-term accuracy. It uses an ordinary
  32.768 kHz crystal, not a temperature-compensated oscillator.
- The VL/lost-power flag tells firmware that stored time may be invalid. It is
  not a battery gauge and does not predict remaining battery capacity.
- The external and onboard RTCs can detect disagreement, but two clocks cannot
  prove which one is correct without recent trusted sync history.
- If the clocks disagree materially and no recent trusted source exists, the
  correct response remains `TIME_INVALID`.

V2 clock health fields should include:

- External RTC presence.
- External RTC VL/lost-power flag.
- External RTC battery installation or replacement date.
- Conservative scheduled battery-replacement interval.
- Last trusted verification date.
- Measured historical drift.
- Difference between external and onboard RTC.

V2 I2C architecture:

- Onboard RTC remains on CoreInk internal I2C.
- External Unit RTC uses the external HY2.0 Grove bus.
- Both chips may use address `0x51`, so they must be on separate buses.
- Run the 20 cm external bus at 50 or 100 kHz, not 400 kHz.
- Set an I2C timeout.
- Implement bus recovery.
- Detect missing or unplugged external RTC.
- Disable unused alarm, timer, or clock-output functions.

V2 wake-source rule:

- The external RTC retains authoritative time but cannot wake CoreInk through
  the Grove connector.
- Onboard RTC or ESP32 timer remains the wake source while CoreInk battery is
  available.
- On wake, CoreInk reads the external RTC and treats it as authoritative only if
  it passes validation.
- Before sleeping, CoreInk can program the onboard RTC/ESP32 wake path for the
  next wake event.

V2 clock-manager rule:

1. Read external RTC.
2. Read onboard RTC.
3. Validate both.
4. Select authoritative time.
5. Set system time.
6. Write trusted network/manual time back to both RTCs.
7. Never allow the secondary RTC to overwrite a valid authoritative source
   automatically.

V2 should not silently downgrade to onboard RTC indefinitely if the external RTC
is configured as installed hardware. Allow a measured short grace period, show a
fault such as `EXTERNAL_RTC_MISSING` or `RTC_DEGRADED`, and require service.

CoreInk unattended restart after complete battery depletion is a go/no-go
hardware acceptance test. The product must prove this sequence repeatedly on
the exact target hardware revision:

1. Internal battery reaches zero.
2. USB power returns without the user pressing PWR.
3. CoreInk starts application firmware.
4. Firmware asserts the power-hold path as early as possible.
5. CoreInk detects whether RTC time survived or enters `TIME_INVALID`.
6. SoftAP and S31 services resume.

If CoreInk does not consistently auto-start after total depletion, onboard-only
V1 needs either a hardware change, a different controller, or an explicit
product limitation requiring manual restart.

## Timekeeping

The onboard BM8563 is the V1 holdover source.

Time-source order:

1. Trusted public NTP, when internet is available.
2. Trusted local NTP, when an offline LAN provides it.
3. Manual time entered directly on the CoreInk.
4. Onboard BM8563 RTC during offline operation.

The system supports three home types:

- Internet-connected home: CoreInk can periodically obtain time from public NTP
  and correct the onboard RTC.
- Offline home with a local router: CoreInk can obtain time from a local NTP
  server.
- Completely offline home: the installer sets time directly on CoreInk, then
  the onboard RTC keeps counting while the CoreInk battery remains alive.

Internet and a router are optional. Neither is required for normal operation.

The first supported product profile is narrower:

- One CoreInk.
- One to four S31 satellites.
- CoreInk onboard RTC only; no external RTC module.
- CoreInk-owned SoftAP/HTTP as the baseline transport.
- ESP-NOW as an alternative S31 control transport only after pairing, channel,
  ACK/retry, payload-size, and RF tests pass.
- HTTP/JSON protocol.
- Manual time setup plus optional trusted NTP.
- Fixed selectable fail-safe choices.
- No cloud features.

CoreInk tracks three explicit time states:

```text
SYNCED
RTC_HOLDOVER
TIME_INVALID
```

`SYNCED` means time was recently confirmed by a trusted source such as NTP or a
deliberate manual setup.

`RTC_HOLDOVER` means no current NTP source is available, but the onboard RTC is
valid and continuing from a previously trusted time.

`TIME_INVALID` means the system cannot safely trust the clock.

Reasons for `TIME_INVALID` include:

- RTC voltage-low flag.
- Impossible date.
- Date outside the supported range.
- RTC time moving backward unexpectedly.
- Corrupted settings.
- RTC never initialized.
- Excessive time since the last trusted verification, depending on product
  policy.

When time is invalid, CoreInk does not generate new calendar-derived
transitions.

CoreInk records RTC health:

- Last successful synchronization.
- Synchronization source.
- Last RTC-to-NTP difference.
- Age of the last synchronization.
- Onboard RTC validity.
- RTC voltage-low status.

When NTP becomes available again, CoreInk can display how far the RTC drifted
before correction.

Clock uncertainty must remain separate from user halachic settings. Candle
lighting offset, havdalah setting, Early Shabbos setting, and product-level
clock uncertainty allowance are separate controls. The firmware must not
silently alter the user's minhag because the RTC has not been checked recently.
When uncertainty becomes unacceptable, the system should show a fault and enter
the configured fail-safe policy.

Initial engineering thresholds:

```text
warn_clock_uncertainty_seconds: 60
max_clock_uncertainty_seconds: 120
```

The estimate should include assumed RTC drift, time since the last trusted
verification, RTC validity flags, and impossible or backward RTC movement. The
60/120 second values are engineering placeholders, not final product policy; a
long-offline product may need larger thresholds plus a visible service warning.
If the hard limit is exceeded, CoreInk enters `TIME_INVALID` rather than
silently adjusting zmanim.

The product should record:

- Onboard RTC initialization date.
- Last trusted clock verification date.
- Last measured RTC drift.
- RTC voltage-low/lost-clock flag status.
- RTC plausibility-check result.
- Manual correction history.

## Calendar And Zmanim Engine

Profile A moves the existing Melacha Plug calendar code to CoreInk.
Profile B keeps that code on every S31 and may also run it on CoreInk as a
reference calculation for disagreement detection.

That includes:

- Hebrew date calculations.
- Shabbos detection.
- Yom Tov detection.
- Eretz Yisrael versus diaspora.
- Solar calculations.
- Latitude and longitude.
- Timezone and daylight-saving handling.
- Early Shabbos.
- Candle-lighting offsets.
- Havdalah methods and offsets.
- Normal mode.
- Inverse mode.
- Overrides.
- Adjacent Shabbos and Yom Tov handling.

The required refactor for both profiles is that the calendar engine must stop
directly touching GPIO. Today, some logic directly manipulates relay state,
button enable/lock, and protected-mode LED. That should become a generic policy
result:

```text
desired_relay_state
physical_button_locked
protected_led_state
reason
effective_from
effective_until
```

The same calendar engine can then target:

- A local relay for legacy standalone builds.
- A remote S31 satellite for Profile A CoreInk-authoritative builds.
- A local S31 relay for Profile B S31-local-brain builds.
- A CoreInk reference calculation for Profile B diagnostics.

The output of the calendar engine is policy, not GPIO.

Internally, generated schedules use UTC timestamps. CoreInk converts to local
time only for calculation input and display. UTC removes ambiguity during
daylight-saving changes.

## CoreInk Interface

CoreInk boots directly into appliance firmware. There is no general-purpose
launcher or ordinary computer interface.

The normal screen can show:

```text
Thursday 9:24 PM
2 Av 5786

Next:
Kitchen OFF
Friday 7:48 PM

Time: RTC HOLDOVER
Plugs: 4/4 confirmed
```

Useful indicators:

- Current Shabbos or weekday status.
- Normal or inverse mode.
- Next transition.
- Reason for the transition.
- Connected S31 count.
- Desired versus confirmed state.
- Onboard RTC status.
- Battery and USB power status.
- Schedule expiry warning.
- Communication errors.

Because the display is e-ink, it remains visible without continuous screen
power. It should update once per minute, on a state change, or when a warning
changes.

Every positive status must include freshness. Do not show permanent-looking
unqualified states such as "4/4 confirmed" or "Clock OK." Use wording like:

```text
4/4 confirmed at 9:21 PM
Status valid until 9:26 PM
```

or show an obvious last-update timestamp on the normal screen.

### CoreInk Inputs

CoreInk does not have a rotary encoder. Its documented controls are discrete:

- Dial Up.
- Dial Middle / Select.
- Dial Down.
- User button.

The setup wizard should be written as up/select/down/back or service-button
navigation. Product copy and firmware text should not say "turn the dial" or
"spin to select."

### E-Ink Configuration

Production ESPHome configuration must use the exact CoreInk display model:

```yaml
model: 1.54in-m5coreink-m09
update_interval: never
```

The BUSY pin must be configured inverted for this display model. Screen updates
should be manual only:

- Meaningful state change.
- Error or warning change.
- Schedule transition.
- Setup screen change.
- At most once per minute when the displayed minute changes.

The visible screen area is small, so detailed timezone, latitude/longitude,
per-plug settings, and diagnostics should primarily live in the local web UI.

### Local Setup Wizard

The CoreInk buttons or selector can provide a strict guided setup:

1. Set year.
2. Set month.
3. Set day.
4. Set hour.
5. Set minute.
6. Set timezone.
7. Configure daylight-saving rules.
8. Enter latitude.
9. Enter longitude.
10. Select Eretz Yisrael or diaspora.
11. Select normal or inverse mode.
12. Set zmanim offsets.
13. Select fail-safe behavior.
14. Confirm and generate schedule.

The user sees every value while changing it.

### Child-Resistant Operation

The normal screen can be read-only. Settings can require a PIN, long press,
hidden button sequence, holding two buttons together, or service mode enabled
only during startup.

During Shabbos or Yom Tov, configuration can become completely read-only
according to product policy.

## Local Web GUI

CoreInk hosts a local web application. A user connects a phone or laptop to the
CoreInk private Wi-Fi network and opens:

```text
http://192.168.4.1
```

No internet is needed.

The web GUI handles configuration that is awkward on the e-ink screen:

- Date and time.
- Timezone and daylight-saving rules.
- Latitude and longitude.
- Eretz Yisrael or diaspora.
- Normal or inverse Shabbos mode.
- Candle-lighting offset.
- Havdalah method.
- Other zmanim adjustments.
- Per-plug names.
- Per-plug relay behavior.
- Per-plug fail-safe behavior.
- Physical button lock rules.
- Schedule preview.
- Hebrew date preview.
- Upcoming transitions.
- S31 connectivity.
- Desired and reported states.
- Cached schedule generation.
- Onboard RTC status.
- Last time synchronization.
- Battery and power status.
- Firmware versions.
- Local firmware upload.
- Configuration backup and restore.
- Diagnostic logs.

The GUI should require an administrator password.

A captive portal can be added, but the fixed IP is more deterministic. Phones
may warn that the Wi-Fi network has no internet. The user chooses to stay
connected.

## Wi-Fi Network

CoreInk creates its own Wi-Fi access point.

Example package credentials:

```text
SSID: Melacha-A7F3
Security: WPA2
Password: unique factory-generated password
CoreInk IP: 192.168.4.1
```

The SSID can be hidden and still use WPA2 security. The hidden SSID is not the
security mechanism; the WPA2 password is.

Each package should have:

- Unique SSID.
- Unique WPA2 password.
- Unique controller ID.
- Unique protocol authentication key.

Each S31 is pre-flashed with:

- Hidden SSID.
- WPA2 password.
- Controller ID.
- Device ID.
- Fixed IP or assigned address.
- Protocol secret.
- Default fail-safe behavior.

The customer does not configure S31 Wi-Fi. It connects automatically.

The ESP32 SoftAP has a practical station ceiling around ten clients, depending
on firmware configuration. The first product should officially support:

- One to four S31 satellites.
- One temporary smartphone connection.
- Connection headroom.

Homes needing more than four plugs should use a separate CoreInk controller in
V1, or wait for a V2 architecture with a dedicated access point or router-based
mode.

Each S31 registers with CoreInk after joining the AP. CoreInk keeps a
device-ID-to-IP table from authenticated registration/status messages. Fixed IPs
or DHCP reservations are allowed, but registration is the source of truth for
the current address.

The CoreInk SoftAP must be explicitly configured and tested for at least:

```text
max_connection: 6
```

This covers four S31s, one setup phone, and one connection of headroom. Test
four S31s continuously connected, a phone joining/leaving, simultaneous S31
reconnection after CoreInk restart, and hidden-SSID reconnect behavior.

Network modes:

- Normal offline mode: AP only.
- Optional synchronization mode: AP+STA for public/local NTP, with S31
  reconnection tested across channel changes.
- V2 Home-LAN mode: not part of V1.

If AP+STA mode is used, the ESP32 SoftAP can follow the external router channel.
The cached S31 schedule should survive brief reconnects, but channel migration
must still be fault-tested.

### ESP-NOW Transport Alternative

ESP-NOW is a viable alternative transport for CoreInk-to-S31 control messages.
It is connectionless Wi-Fi peer-to-peer messaging between Espressif devices, so
the S31 control link does not need an access point, DHCP, IP addresses, DNS, or
HTTP.

ESP-NOW is not automatically mesh. It does not by itself provide forwarding,
routing, or repeater behavior. If a far S31 cannot hear CoreInk directly, the
product must either move the CoreInk, add a real repeater/forwarding protocol,
or use a distinct mesh framework. Do not describe ESP-NOW as mesh unless that
forwarding layer has actually been implemented and tested.

Recommended use:

- Keep CoreInk SoftAP/HTTP for phone setup and firmware upload.
- Allow ESP-NOW as the S31 control transport for time, settings, ACK, and status
  messages.
- Keep SoftAP/HTTP as the simpler baseline transport until ESP-NOW reliability,
  pairing, channel behavior, and diagnostics are tested.

ESP-NOW implementation constraints:

- Pair by MAC address plus `controller_id`, `controller_epoch`, and protocol
  secret.
- Use application-level HMAC, sequence numbers, ACKs, retries, and duplicate
  suppression. MAC-layer send success is not enough to prove the application
  processed the message.
- Assume small payloads for S31 compatibility unless measured otherwise. The
  safest ESP8266-compatible design keeps packets at or below 250 bytes, or uses
  explicit fragmentation.
- Do not send large JSON schedules over ESP-NOW unless fragmentation,
  reassembly, timeout, and flash atomicity are specified.
- Prefer compact CBOR/TLV/binary frames for ESP-NOW. HTTP/JSON remains easier
  for debugging and first bring-up.
- Broadcast discovery must not carry secrets. Encrypted unicast or an
  application HMAC is required for trusted time/settings.
- All devices must share a radio channel. If CoreInk also runs SoftAP or AP+STA,
  channel changes must be tested because ESP-NOW peers must transmit on the
  active channel.
- ESP-NOW range must be tested in the real mounted location; it removes IP
  networking but does not remove RF physics.

Profile fit:

- Profile A over ESP-NOW is possible, but full schedule packets may be too large
  unless compressed or fragmented.
- Profile B over ESP-NOW is a cleaner fit because CoreInk sends compact time and
  settings updates, while each S31 calculates its own schedule locally.

## Local Time Server

The S31 does not have a battery-backed RTC. After an S31 reboot or power outage,
it cannot safely execute absolute UTC transitions until it knows the current
time.

CoreInk provides a local NTP service or a dedicated authenticated time endpoint.
The authenticated HTTP time endpoint is preferred for V1. If local NTP is used,
it is rough time only; authenticated schedule validation still decides whether
relay policy may execute.

Startup flow:

1. CoreInk boots.
2. CoreInk reads and validates the onboard RTC.
3. CoreInk establishes system time.
4. CoreInk starts its Wi-Fi access point.
5. CoreInk starts local time service.
6. S31 connects.
7. S31 obtains valid time.
8. Profile A: CoreInk sends the current schedule.
9. Profile B: CoreInk sends the current settings snapshot and S31 calculates
   policy locally.
10. S31 applies the current desired state.
11. S31 confirms the result or reports calculated policy.

An S31 that has not obtained valid time stays in its configured fail-safe state.

## Profile A CoreInk To S31 Schedule Contract

The detailed packet rules belong in `COREINK_S31_PROTOCOL.md`.

In Profile A, CoreInk must not merely say "turn on now." It sends an
authoritative rolling schedule that the S31 can survive on.

Protocol rules:

- Never send toggle.
- Always send an absolute desired state.
- Schedule packets replace older schedules rather than patching them.
- `controller_epoch` identifies the pairing lifetime.
- `controller_epoch` is generated from at least 128 cryptographic random bits,
  not from a timestamp or short suffix.
- `schedule_generation` only increases within the current `controller_epoch`.
- `controller_epoch` and `schedule_generation` survive CoreInk reboot.
- The same packet may be sent repeatedly without changing the result.
- A lower `schedule_generation` is rejected as stale.
- The same `schedule_generation` with the same CRC is accepted as a duplicate.
- The same `schedule_generation` with different contents is rejected.
- CRC detects accidental corruption.
- HMAC authenticates the packet using a pre-shared secret and is mandatory for
  product firmware.
- CRC and HMAC are computed over canonical JSON bytes, not over arbitrary JSON
  formatting.
- HTTP body sizes, transition counts, JSON depth, and string lengths have fixed
  limits so malformed packets cannot exhaust ESP8266 memory.
- All execution timestamps are UTC.
- Every schedule has a validity window.
- Every S31 stores the accepted schedule in flash.
- CoreInk reconciles state periodically and after reconnection.
- Failed requests retry with bounded backoff.

An S31 with invalid time may still authenticate and cache a newer schedule based
on controller identity, epoch, `schedule_generation`, CRC, HMAC, and structure.
It must report that schedule as stored pending time and must not apply
`current_policy` or timestamped transitions until it has valid time and can
re-check freshness.

JSON over HTTP is the easiest first dependable implementation. CBOR or compact
binary can be considered later if firmware size or parsing cost requires it.

The no-broker design avoids MQTT and another failure point. For the hardened
version, use a small custom protocol rather than depending heavily on ESPHome's
generic entity REST identifiers.

## Profile B CoreInk To S31 Settings Contract

In Profile B, CoreInk sends time and full settings snapshots instead of rolling
schedules. Each S31 calculates its own schedule locally.

Profile B rules:

- Never patch settings. Send a full replacement settings snapshot.
- `settings_generation` only increases within the current `controller_epoch`.
- A lower `settings_generation` is rejected as stale.
- The same `settings_generation` with the same CRC/HMAC is accepted as a
  duplicate.
- The same `settings_generation` with different contents is rejected.
- S31 may store authenticated settings while time is invalid, but must not apply
  calendar-derived relay policy until time and settings freshness are valid.
- S31 reports calculated current policy, next transition, reason,
  `settings_generation`, `calendar_engine_version`, and calculation fingerprint.
- CoreInk displays disagreement between S31 reports instead of silently choosing
  one plug as correct.
- CoreInk can re-push settings, request a detailed calculation report, or require
  service if disagreement persists.

Profile B is the natural fit for ESP-NOW because time/settings/status frames can
be much smaller than complete rolling schedules.

## Acknowledgements And Reporting

The S31 must distinguish receiving controller data from actually applying relay
policy.

Useful acknowledgements include:

```text
boot_id
device_id
controller_id
accepted_schedule_generation
accepted_settings_generation
cached_schedule_generation
cached_settings_generation
calendar_engine_version
calculation_fingerprint
valid_until_utc
time_valid
time_source
current_utc
last_transition_id
last_transition_result
next_transition_id
next_transition_reason
requested_relay_state
relay_output_state
physical_button_locked
protected_led
last_controller_seen_utc
```

Rejection reasons include:

```text
stale_generation
bad_crc
bad_hmac
wrong_controller
expired_schedule
unsupported_protocol
invalid_transition_order
flash_write_failed
```

The S31 can confirm that firmware set the relay GPIO to the requested state. It
cannot independently prove the mechanical relay contacts physically closed
without separate contact feedback. The GUI should label this honestly:

```text
Relay output confirmed
Not physical contact independently verified
```

S31 hardware requirements for the Sonoff S31 profile:

- `early_pin_init: false`.
- GPIO12 for relay and relay-associated LED.
- GPIO13 for the independent green/protected-mode LED.
- GPIO0 for the button.
- UART logging disabled if CSE7766 power monitoring is enabled.
- CSE7766 at 4800 baud, even parity.
- Small custom HTTP endpoints; avoid the generic ESPHome `web_server` on S31
  for the satellite runtime.

The GPIO13 green LED is the protected-mode indicator. The LED associated with
GPIO12 follows the relay and is not an independent status indicator.

## Profile A Cached Schedule Behavior

Each S31 stores a rolling future schedule.

Initial target:

- At least the next 8 transitions.
- Or at least 14 days.
- Whichever covers more time.

That cached horizon is not the same as the autonomous execution validity window.
V1 should store the longer horizon but require refresh sooner:

```text
refresh_required_by_utc: about 24 hours after schedule creation
valid_until_utc: about 48 hours after schedule creation
transition_horizon_until_utc: about 14 days or 8 transitions
```

After the refresh deadline, CoreInk should warn and retry aggressively. After
the hard validity deadline, S31 enters fail-safe unless an explicit grace policy
allows continued cached execution.

`current_policy` expires at `valid_until_utc`. After reconnect or time recovery,
the S31 applies the latest effective policy at the current UTC time; it does not
replay every missed transition.

Each cached schedule includes:

- UTC transition timestamps.
- Desired relay state.
- Physical button lock state.
- Protected LED state.
- Reason.
- `controller_epoch`.
- `schedule_generation`.
- Creation time.
- Expiry time.
- CRC.
- Last confirmed transition.

The cached schedule protects against temporary loss of CoreInk or Wi-Fi. It does
not solve a complete S31 power loss by itself, because after reboot the S31 has
no valid clock. It must first obtain time from CoreInk.

## Profile B Cached Settings Behavior

Each S31 stores the newest accepted settings snapshot and uses it to calculate
policy locally.

Each cached settings snapshot includes:

- Timezone and daylight-saving behavior.
- Latitude and longitude.
- Eretz Yisrael or diaspora mode.
- Normal or inverse mode.
- Early Shabbos policy.
- Zmanim mode and values.
- Candle-lighting and havdalah settings.
- Per-plug relay behavior.
- Button-lock behavior.
- Fail-safe policy.
- Clock-uncertainty policy.
- `controller_epoch`.
- `settings_generation`.
- Creation time.
- Refresh deadline.
- Hard expiry time.
- CRC/HMAC.
- Calendar-engine compatibility requirement.

The cached settings snapshot protects against temporary loss of CoreInk or
Wi-Fi. It also allows S31 to keep functioning after CoreInk disappears, as long
as S31 remains powered, its software clock remains within the configured
uncertainty window, and the settings snapshot has not expired.

It does not solve a complete S31 power loss by itself. After reboot, S31 has no
valid clock and must obtain authenticated time before applying calendar-derived
policy.

## Failure Behavior

These states are non-negotiable. Profile A uses downloaded schedules. Profile B
uses downloaded settings plus S31-local calendar calculation.

### CoreInk Online, Time Valid, S31 Online

Profile A: CoreInk is schedule-authoritative. The S31 obtains time, accepts the
newest schedule, applies the current policy, caches future transitions, reports
its state, and executes future transitions locally.

Profile B: CoreInk is time/settings-authoritative. The S31 obtains time, accepts
the newest settings snapshot, calculates policy locally, applies its calculated
current policy, and reports the calculation back to CoreInk.

### CoreInk Offline, S31 Powered, S31 Time Valid, Schedule Valid

The S31 continues executing the cached schedule. This covers Wi-Fi interruption,
CoreInk reboot, temporary CoreInk firmware failure, or CoreInk USB removal while
the S31 still has AC power.

Profile B equivalent: the S31 continues calculating and executing its local
calendar policy while its time, settings snapshot, and uncertainty window remain
valid.

### CoreInk Offline And Cached Schedule Expires

The S31 enters the configured fail-safe state.

Profile B equivalent: if the settings snapshot expires or S31 clock uncertainty
exceeds policy, the S31 enters the configured fail-safe state.

Recommended default:

- Execute the valid cached schedule.
- After expiry, enter the configured protected state.
- Lock the physical button.
- Show protected-mode LED indication.

### S31 Reboots And Has No Valid Time

The S31 cannot safely use absolute UTC timestamps. It immediately enters the
configured fail-safe Shabbos state until it receives valid time and either:

- Profile A: receives a valid schedule and applies the current authoritative
  policy.
- Profile B: has valid settings, calculates current policy locally, and applies
  that calculated policy.

The application-level relay boot default should be configured to the fail-safe
state so there is no unsafe temporary state after firmware GPIO initialization
while networking starts.

Button and relay safety have a pre-application caveat. GPIO0 is also the ESP8266
boot strap used for flashing. Holding the S31 button during power-up can prevent
normal firmware boot before application code can lock the button. Firmware
lockout applies only after application startup. A physical button cover,
recessed bracket, or enclosure is required if child resistance must include
power-up manipulation.

Before application firmware initializes GPIO, the relay cannot be guaranteed to
follow product policy. The hardware-safe expectation is relay off. A configured
fail-safe ON cannot be guaranteed from the first instant after power
restoration; this matters for inverse mode.

### CoreInk Time Becomes Invalid

CoreInk shows a prominent clock fault, exposes the fault in the web GUI, allows
manual correction, and reports `TIME_INVALID` to S31s.

Profile A: CoreInk stops generating new calendar-derived transitions and does
not send speculative schedules.

Profile B: CoreInk stops sending trusted time updates or new settings snapshots
that depend on the invalid clock.

Profile A: each S31 continues a still-valid previously accepted schedule and
enters fail-safe when that schedule expires.

Profile B: each S31 continues using still-valid previously accepted settings and
its own valid local time estimate. It enters fail-safe when settings expire or
its local time uncertainty exceeds policy.

### Corrupted Or Stale Schedule

The S31 rejects it and keeps the newest previously validated schedule.

Profile B equivalent: corrupted, unauthenticated, stale, or conflicting settings
snapshots are rejected, and the newest previously validated settings remain
active until they expire.

### CoreInk Returns After Outage

The S31 reports boot ID, time validity, current relay output,
`cached_schedule_generation`, last applied transition, and last controller seen.

CoreInk then confirms the existing schedule or replaces it with a newer
`schedule_generation`.

Profile B equivalent: S31 reports boot ID, time validity, current relay output,
`cached_settings_generation`, calendar-engine version, calculated current policy,
next transition, and last controller seen. CoreInk then confirms the settings
generation, replaces it with a newer one, or surfaces disagreement.

### Settings Change

In Profile A, any change affecting zmanim or behavior causes CoreInk to:

1. Increment `schedule_generation`.
2. Recalculate the entire rolling schedule.
3. Replace the old schedule on every S31.
4. Wait for acceptance acknowledgements.
5. Show which plugs are confirmed.

In Profile B, the same kind of change causes CoreInk to:

1. Increment `settings_generation`.
2. Serialize a complete replacement settings snapshot.
3. Replace the old settings on every S31.
4. Wait for settings acknowledgements.
5. Wait for calculated policy reports.
6. Show confirmed plugs and any calculation disagreements.

## Normal And Inverse Modes

The calendar engine determines whether the current period is protected. Each
plug then maps that protected state to its configured relay behavior.

Existing standalone behavior:

- Normal mode: protected period turns the relay off; weekday turns it on.
- Inverse mode: protected period turns the relay on; weekday turns it off.

The CoreInk design should preserve that behavior as policy:

```text
protected_period -> desired_relay_state based on per-plug mode
protected_period -> physical_button_locked true
protected_period -> protected_led_state true
weekday -> desired_relay_state based on per-plug mode
weekday -> physical_button_locked false
weekday -> protected_led_state false
```

Per-plug configuration should make the behavior explicit so users understand
whether a protected period energizes or de-energizes the appliance.

## Hardware And Environment Limits

The completed product should inherit the narrower CoreInk operating range:

```text
0 C to 60 C
US 120 V / 60 Hz S31 profile only
```

The wall tap must provide stable continuous 5 V power for CoreInk and retain a
properly grounded, certified pass-through outlet. Do not represent this V1 as a
universal-voltage product.

Mechanical, electrical, thermal, and RF tests:

- Blocking the second wall receptacle.
- Excessive torque on the wall outlet.
- USB cable strain.
- Adhesive or bracket failure under heat.
- CoreInk battery temperature.
- Onboard RTC drift near the wall tap and active S31.
- Appliance inrush causing CoreInk brownouts.
- Wi-Fi range from the mounted location.
- Range to S31 satellites across a real home.
- Actual appliance inrush and duty cycle, not only the S31 nameplate rating.

## Go/No-Go Hardware Tests

Before treating the hardware selection as final:

1. Total-depletion recovery: fully deplete CoreInk battery, restore USB without
   touching PWR, and prove unattended boot repeatedly.
2. Wake behavior: prove any planned low-power sleep/wake path using onboard RTC
   or ESP32 timer, and confirm application resumes S31 services.
3. RTC accuracy: measure several CoreInk onboard RTCs over 30, 60, and 90 days,
   including warm operation beside an active S31.
4. S31 boot safety: cold boots, watchdog resets, rapid power cycles,
   button-held boots, and Wi-Fi failures while recording relay and LED behavior.
5. Client capacity: four S31s plus a phone, simultaneous reconnection, hidden
   SSID reconnect, and AP+STA channel changes if sync mode exists.
6. E-ink safety: exact model, inverted BUSY pin, event-driven refresh, and
   stale-status labeling.
7. Load and environment: actual appliances, inrush, continuous load,
   temperature, USB stability, RF range, and receptacle strain.
8. Flash atomicity: remove power during schedule, key, epoch, and generation
   writes on both devices.
9. S31 clock drift: measure its unsynchronized software clock through the full
   proposed autonomous validity window.
10. RTC failure: simulate onboard RTC lost-clock/VL, corrupt time, impossible
   date, and full CoreInk battery depletion.
