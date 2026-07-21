# Parallel Device Architecture Plan

## Goal

Support many Melacha-aware devices as true parallel firmware builds:

- Smart plugs.
- Wall switches.
- Energy-monitoring plugs.
- ESP8266 and ESP32/ESP32-C3 boards.
- Optional features such as Bluetooth Proxy, ESP-NOW, web UI, and offline
  CoreInk coordination.

The maintainer goal is one shared product brain with many hardware targets, not
many copied firmware forks.

The architecture should make this distinction explicit:

- **Melacha Plug Core**: protected-period policy based on Shabbos and Yom Tov.
- **Shabbos Timer**: user schedules based on clock time or zmanim.
- **Hardware targets**: plug/switch model pinouts and board-specific features.
- **Feature modules**: energy monitoring, Bluetooth Proxy, ESP-NOW, diagnostics,
  setup mode, and transport integrations.

## Product Concepts

### Melacha Plug Core

Melacha Plug Core decides whether the device is in a protected period and what
lockout policy should apply.

It owns:

- Current time validity.
- Location, timezone, and daylight-saving context.
- Hebrew calendar.
- Shabbos and Yom Tov detection.
- Eretz Yisrael / diaspora behavior.
- Early Shabbos behavior.
- Start/end offsets and zmanim mode.
- Normal/inverse protected-period relay policy.
- Physical button lock policy.
- Protected-mode LED policy.

It must not directly touch GPIO. It should produce a policy result:

```text
protected_period: true/false
desired_relay_state: ON/OFF/UNCHANGED
physical_button_locked: true/false
protected_led_state: ON/OFF/FLASH/UNCHANGED
reason: shabbos/yom_tov/weekday/invalid_time/manual_override
effective_from
effective_until
```

This lets the same core target a Sonoff S31, Athom wall switch, Athom smart
plug, KAUF plug, CloudFree plug, or a future relay board.

### Shabbos Timer

Shabbos Timer is the schedule engine. It answers: "when should this relay turn
on or off according to user-defined schedule edges?"

It owns:

- Clock schedules.
- Calendar/zmanim schedules.
- Repeating rules.
- Schedule priority.
- Offset handling, including offsets that cross civil midnight.
- Missed-transition recovery after reboot or late time sync.
- Current winning schedule edge.

It should produce schedule intents:

```text
intent_source: schedule
requested_relay_state: ON/OFF
effective_at
schedule_id
reason: clock_edge/zman_edge/recovery
respect_melacha_lockout: true/false
```

Shabbos Timer is not the halachic lockout engine. It can request relay changes
during Shabbos/Yom Tov only when the configured schedule policy allows it. Live
manual commands remain blocked during protected periods.

### Command Arbiter

The command arbiter is the single place where relay intents become relay output.

Inputs:

- Melacha Plug Core policy.
- Shabbos Timer schedule intents.
- Physical button actions.
- Web UI and captive portal actions.
- Home Assistant/API actions.
- ESP-NOW actions.
- Energy/current-limit safety trips.
- Restore/fail-safe policy.

Outputs:

- Apply relay output.
- Reject/ignore a command.
- Report diagnostic reason.

Required behavior:

- Raw relay GPIO must be internal wherever ESPHome allows it.
- Every command path must call the same guarded relay script/API.
- During Shabbos/Yom Tov, live manual commands are ignored or rejected.
- Commands received while locked are not replayed after lockout exits.
- Weekday manual state is not overwritten every minute by periodic calendar
  checks.
- Safety trips can always force a safer relay state.

Suggested priority order:

| Priority | Source | Rule |
| --- | --- | --- |
| 1 | Hardware/bootstrap fail-safe | Safest known relay/button/LED defaults |
| 2 | Safety trip | Over-current or other protective action |
| 3 | Invalid time fail-safe | Configured safe state until time is valid |
| 4 | Melacha protected-period lockout | Blocks live commands; may force protected relay policy |
| 5 | Allowed preconfigured schedule edge | Applies only if allowed by schedule lockout setting |
| 6 | Manual/API/web/ESP-NOW command | Allowed only outside protected lockout |
| 7 | Restore/default | Startup fallback when no stronger source exists |

## Firmware Package Layout

Use thin target files and shared packages.

```text
esphome/
  targets/
    melachaplug-sonoff-s31.yaml
    melachaplug-kauf-plf10.yaml
    melachaplug-cloudfree-p2.yaml
    melachaplug-athom-sw13-us-switch.yaml
    melachaplug-athom-pg03v3-us16a.yaml

  packages/
    core/
      device_base.yaml
      melacha_core.yaml
      shabbos_policy.yaml
      guarded_relay.yaml
      ntp.yaml
      location.yaml
      web_ui.yaml

    timer/
      schedule_engine.yaml
      clock_edges.yaml
      zman_edges.yaml
      schedule_storage.yaml
      schedule_diagnostics.yaml

    hardware/
      sonoff_s31.yaml
      kauf_plf10.yaml
      cloudfree_p2.yaml
      athom_sw13_us_switch.yaml
      athom_pg03v3_us16a.yaml

    features/
      energy_cse7766.yaml
      energy_hlw8032.yaml
      bluetooth_proxy.yaml
      espnow_transport.yaml
      wifi_setup_button.yaml
      current_limit.yaml
      coreink_satellite.yaml

    profiles/
      melacha_plug_core_only.yaml
      melacha_plug_with_timer.yaml
      satellite_profile_a.yaml
      satellite_profile_b.yaml
```

Each target file should mostly declare identity, select hardware, and include
profiles/features.

Example:

```yaml
substitutions:
  device_name: melachaplug-athom-pg03v3-us16a
  friendly_name: Melacha Plug Athom PG03V3 US

packages:
  hardware: !include ../packages/hardware/athom_pg03v3_us16a.yaml
  profile: !include ../packages/profiles/melacha_plug_with_timer.yaml
  energy: !include ../packages/features/energy_cse7766.yaml
  bluetooth_proxy: !include ../packages/features/bluetooth_proxy.yaml
```

## Hardware Contract

Every hardware package should provide the same logical contract.

Required fields:

| Contract item | Meaning |
| --- | --- |
| `relay_output_id` | Internal relay GPIO switch or output |
| `primary_button_id` | Physical button binary sensor |
| `protected_led_id` | LED used for protected-mode indication, if present |
| `relay_restore_mode` | Hardware-specific safe/default restore mode |
| `button_inverted` | Physical button polarity |
| `led_inverted` | LED polarity |
| `board` | ESPHome board/platform settings |

Optional capabilities:

| Capability | Examples |
| --- | --- |
| `has_energy_monitoring` | S31 CSE7766, Athom PG03V3 CSE7766/HLW8032 |
| `has_bluetooth_proxy` | ESP32-C3 targets |
| `has_espnow` | ESP32/ESP32-C3 targets |
| `has_independent_protected_led` | S31 green LED |
| `is_wall_switch` | Athom SW13 wall switch |
| `is_plug` | S31, KAUF, CloudFree, Athom PG03V3 |

Hardware packages should not contain calendar logic. They only adapt pins,
board settings, and optional sensors to the shared contracts.

## Build Matrix

True parallel support means one compiled artifact per supported target/profile.

Suggested build dimensions:

| Dimension | Values |
| --- | --- |
| Hardware | S31, KAUF PLF10/PLF12, CloudFree P2, Athom SW13, Athom PG03V3 |
| Product profile | Core-only, Core+Timer, CoreInk satellite A, CoreInk satellite B |
| Zmanim flavor | Degree mode, minutes mode, or unified runtime mode |
| Feature flags | Energy, Bluetooth Proxy, ESP-NOW, web UI |
| Chip family | ESP8266/ESP8285, ESP32, ESP32-C3 |

Release artifacts should be explicit:

```text
melachaplug-sonoff-s31-core-2026.07.21.bin
melachaplug-sonoff-s31-timer-2026.07.21.bin
melachaplug-athom-sw13-us-switch-timer-2026.07.21.bin
melachaplug-athom-pg03v3-us16a-timer-energy-btproxy-2026.07.21.bin
```

Do not try to ship one universal firmware binary across unrelated plug models.
The shared code is universal; the compiled firmware artifacts are not.

## Profiles

### Core-Only Profile

Use case: classic Melacha Plug behavior.

Behavior:

- Protected period starts: relay follows normal/inverse protected policy.
- Physical button locks.
- Protected LED indicates lockout.
- Protected period ends: relay follows configured weekday behavior or preserves
  manual state, depending on final product setting.
- No user schedule engine.

### Core+Timer Profile

Use case: Shabbos Timer plus Melacha lockout.

Behavior:

- Melacha Plug Core determines protected period and live-command lockout.
- Shabbos Timer produces scheduled ON/OFF edges.
- Schedule edges can be allowed or suppressed during protected periods based on
  `respect_melacha_lockout`.
- Weekday manual state persists until a later explicit event.

This should become the main ESP32-C3 plug/switch profile.

### CoreInk Satellite Profile A

Use case: CoreInk is schedule authority.

Behavior:

- CoreInk calculates calendar and schedules.
- Satellite executes signed/cached relay policy.
- Satellite reports state and lockout.
- Satellite does not independently calculate Jewish calendar.

### CoreInk Satellite Profile B

Use case: each satellite keeps local Melacha brain.

Behavior:

- CoreInk distributes time/settings/status UI.
- Satellite calculates protected periods and next transitions locally.
- CoreInk detects disagreement between devices.

## Device Families

### Smart Plugs

Examples:

- Sonoff S31.
- KAUF PLF10/PLF12.
- CloudFree P2.
- Athom PG03V3 US16A.

Expected behavior:

- Local relay controls the receptacle.
- Button is service/manual input.
- Energy monitoring is optional.
- Bluetooth Proxy is optional on ESP32-C3 targets.
- Current-limit features must use the guarded relay path.

### Wall Switches

Example:

- Athom SW13 US wall switch.

Expected behavior:

- Physical wall action is a user command outside protected periods.
- During protected periods the switch input is ignored.
- Weekday wall state should not be overwritten by minute checks.
- Schedules can still run as preconfigured policy if enabled.

Wall switches deserve stricter UX wording because users expect the paddle to
behave like a normal switch on weekdays and do nothing during Shabbos/Yom Tov.

## Testing Requirements

Every supported target must pass compile validation. Text review is not enough.

Minimum matrix before merge:

- At least one ESP8266/ESP8285 target.
- At least one ESP32 target if still supported.
- At least one ESP32-C3 target.
- Every changed hardware package.
- Every changed profile package.

Behavior tests to run on real hardware before release:

- Boot relay safe/default state.
- Button toggles on weekday.
- Button ignored during protected period.
- API/web relay command allowed on weekday.
- API/web relay command rejected during protected period.
- Periodic Melacha check does not overwrite weekday manual state.
- Protected-period transition applies expected relay policy.
- Shabbos Timer edge applies outside protected period.
- Shabbos Timer edge during protected period follows `respect_melacha_lockout`.
- Commands received while locked are not replayed after lockout exits.
- Energy/current-limit trip uses guarded relay path.
- Bluetooth Proxy does not destabilize Wi-Fi/API on ESP32-C3.

## Migration Plan

1. Add this architecture doc and stop adding model-specific behavior directly to
   shared calendar code.
2. Create `targets/` and `packages/` directories while keeping existing YAML
   files as compatibility wrappers.
3. Extract raw GPIO definitions from existing device YAMLs into hardware
   packages.
4. Introduce guarded relay service/script and make new targets use it first.
5. Change button handling to call the guarded relay path.
6. Change web/API exposed relay control to call the guarded relay path or expose
   only a guarded template switch.
7. Refactor Melacha checks to publish policy and only act on transitions or
   policy changes, not every minute.
8. Add Shabbos Timer as a separate schedule engine package.
9. Add build scripts/CI matrix for all supported targets.
10. Produce release artifacts per hardware/profile.

## Maintainer Rules

- Never copy the full firmware for a new plug model.
- Never let hardware packages contain calendar decisions.
- Never expose raw relay GPIO as a writable public control on new targets.
- Never allow a new command path to bypass the command arbiter.
- Treat ESP-NOW, Home Assistant, web UI, BLE-derived automations, and button
  clicks as different transports into the same relay command contract.
- Keep product wording precise: Melacha Plug Core is protected-period lockout;
  Shabbos Timer is scheduled appliance automation.

## Acceptance Criteria

- [ ] New device models can be added by writing a hardware package and target
      YAML, not by copying a full device file.
- [ ] Melacha Plug Core can run without Shabbos Timer.
- [ ] Shabbos Timer can be included only on targets/profiles that support it.
- [ ] All relay command sources use one guarded relay path.
- [ ] The build matrix compiles every supported target/profile.
- [ ] Release artifacts are named by hardware model, profile, and version.
- [ ] The Sonoff S31 path remains supported while Athom ESP32-C3 plug/switch
      targets are added in parallel.

