# Athom ESP32-C3 US Plug Parallel Support Plan

## Goal

Add the Athom **ESP32-C3 US Plug Made For ESPHome** as a parallel Melacha Plug
hardware target without disturbing the current Sonoff S31, CloudFree/KAUF, or
Athom wall-switch paths.

The plug is useful because it combines:

- Local relay control.
- Power and energy monitoring.
- ESP32-C3 4MB flash.
- Bluetooth Proxy support.
- OTA updates through ESPHome.

This target should reuse the same Melacha calendar, Shabbos/Yom Tov lockout,
schedule, and guarded relay semantics as the rest of the project. Energy
monitoring and Bluetooth Proxy are additional capabilities; they must not become
unguarded ways to control the relay.

The broader multi-device structure is defined in
[`PARALLEL_DEVICE_ARCHITECTURE_PLAN.md`](./PARALLEL_DEVICE_ARCHITECTURE_PLAN.md).

## Target Hardware

Primary device: **Athom Smart Plug US V3 / ESP32-C3 US Plug Made For ESPHome**.

| Item | Value |
| --- | --- |
| SKU | `PG03V3-US16A-ESP-1` |
| Product page | [`ESP32-C3 US Plug Made For ESPHome`](https://www.athom.tech/blank-1/esp32-c3-us-plug-for-esphome) |
| Plug type | US plug |
| MCU | ESP32-C3 |
| Flash | 4MB |
| Rated voltage | 100-250V AC |
| Max current/load | 16A resistive load |
| Relay | 16A relay |
| Wireless | 2.4GHz Wi-Fi + Bluetooth LE |
| Bluetooth Proxy | Supported; Athom lists it as enabled by default |
| Energy monitoring | Product page lists HLW8032; upstream config currently uses `cse7766` |
| Upstream ESPHome config | [`athom-smart-plug.yaml`](https://github.com/athom-tech/esp32-configs/blob/main/athom-smart-plug.yaml) |
| ESPHome Devices entry | [`athom-smart-plug-pg03v3-us16a`](https://devices.esphome.io/devices/athom-smart-plug-pg03v3-us16a/) |

Known upstream pin mapping from ESPHome Devices and Athom's current YAML:

| GPIO | Function |
| --- | --- |
| `GPIO5` | Relay |
| `GPIO3` | Button |
| `GPIO6` | LED |
| `GPIO20` | Energy-meter UART RX |

There is one detail to validate when the plug arrives: the product page says
HLW8032, while the current upstream YAML uses ESPHome's `cse7766` sensor
component on `GPIO20` at 4800 baud with even parity. Treat the metering chip and
ESPHome component as a hardware-validation item before cutting firmware.

## Parallel Add Strategy

The new plug should be added as another thin device YAML plus optional hardware
packages. Avoid forking the Melacha calendar engine.

Planned files:

| File | Purpose |
| --- | --- |
| `esphome/melachaplug-athom-pg03v3-us16a.yaml` | Device profile for this plug |
| `esphome/plugins/melachaplug/athom_pg03v3_power.yaml` | Power/energy sensors and current-limit policy |
| `esphome/plugins/melachaplug/esp32c3_bluetooth_proxy.yaml` | Bluetooth Proxy defaults for ESP32-C3 builds |
| Shared guarded relay package/script | Single relay command path used by button, schedules, API/web, ESP-NOW, and current-limit actions |

The current shared `plug_info.yaml` exposes `relay_1` as a raw GPIO switch. That
is acceptable for older direct-relay builds, but this plug should move toward a
guarded relay setter before exposing more command sources. The raw relay GPIO
should become internal, with a template or script API enforcing Melacha lockout.

## Firmware Behavior

### Relay and Button

- Weekday physical button presses toggle the plug normally.
- Weekday manual state remains in effect until another explicit user command,
  schedule edge, protected-period transition, current-limit trip, or reboot
  restore rule.
- The periodic Melacha check must not blindly force the relay back to weekday
  ON/OFF every minute.
- During Shabbos/Yom Tov, physical button presses are ignored.
- During Shabbos/Yom Tov, live relay commands from web UI, Home Assistant/API,
  ESP-NOW, or energy-triggered automations are ignored or rejected.
- Preconfigured schedule/policy edges may still run during Shabbos/Yom Tov only
  when the product settings explicitly allow that behavior.
- Commands received while locked are not replayed after Shabbos/Yom Tov exits.

### Energy Monitoring

Expose the expected Home Assistant sensor set:

- Voltage.
- Current.
- Active power.
- Energy/total energy.
- Total daily energy.
- Optional apparent power, reactive power, and power factor if the validated
  metering component exposes them cleanly.

Energy sensors are read-only observability. Any automation based on power or
current must call the same guarded relay setter as every other relay command.

The 16A current limit should be implemented as a safety policy. If current
exceeds the configured limit, turn the relay off through the guarded path and
publish a diagnostic reason. This action is a protective trip, not a user
schedule edge.

### Bluetooth Proxy

Bluetooth Proxy can remain enabled as a service feature, but it is not relay
authority.

Recommended defaults:

- Use ESPHome's `bluetooth_proxy` with active mode matching upstream Athom
  behavior.
- Gate BLE scanning the way upstream Athom does: start scanning when the native
  API has a connected client and stop scanning when no API clients remain.
- Monitor stability on ESP32-C3 because Wi-Fi, API, BLE scanning, web server,
  and energy metering all share a small device.
- Do not let BLE-originated Home Assistant automations bypass Melacha lockout;
  by the time they reach the plug as a relay command, they are live commands and
  must pass through the guarded relay path.

### ESP-NOW

ESP-NOW remains optional. The ESP32-C3 hardware can support it, but the baseline
bring-up should use Wi-Fi/API/OTA first.

If ESP-NOW is added later:

- Use it only as a transport for idempotent command/status messages.
- Keep Wi-Fi channel constraints explicit.
- Require ACK/retry behavior for relay policy messages.
- Route all ESP-NOW relay requests through the same guarded relay setter.

## Bring-Up Steps

1. Adopt or fetch the upstream Athom YAML for `athom-smart-plug.yaml`.
2. Confirm the shipped device reports ESP32-C3 with 4MB flash.
3. Confirm actual entities in Home Assistant: relay, button, LED, voltage,
   current, power, energy, Bluetooth Proxy status, and OTA/update entities.
4. Validate the metering component:
   - Product page says HLW8032.
   - Current upstream YAML uses `cse7766`.
   - Confirm which ESPHome component produces sane readings on the actual plug.
5. Test relay GPIO `GPIO5`, button `GPIO3`, LED `GPIO6`, and energy UART RX
   `GPIO20`.
6. Build a Melacha Plug device YAML using the existing packages plus the new
   plug-specific energy/Bluetooth packages.
7. Compile with ESP32-C3 4MB settings and no serial UART logging conflict.
8. Flash OTA only after the YAML matches the shipped hardware.
9. Run weekday relay/button tests.
10. Run Shabbos/Yom Tov lockout tests for button, API/web, schedule edges,
    energy-triggered actions, and optional ESP-NOW commands.
11. Run power-monitoring sanity checks with known low and moderate loads.

Do not open the plug or touch internal pins while it is connected to mains.
Serial validation, if ever needed, must be done only with the device fully
disconnected from mains and handled as mains-voltage hardware.

## Acceptance Criteria

- [ ] Device profile compiles for ESP32-C3 with 4MB flash.
- [ ] Relay, button, LED, OTA, web/API, and fallback setup work.
- [ ] Energy sensors publish stable voltage/current/power/energy readings.
- [ ] Bluetooth Proxy remains available without destabilizing Wi-Fi/API.
- [ ] Weekday button toggles are not overwritten by periodic Melacha checks.
- [ ] Shabbos/Yom Tov lockout blocks live button, web/API, Home Assistant,
      ESP-NOW, and energy-triggered relay commands.
- [ ] Preconfigured schedule behavior during Shabbos/Yom Tov follows the
      explicit schedule lockout setting.
- [ ] Current-limit trip turns the relay off and records/reports its reason.
- [ ] The metering-chip mismatch is resolved in the final YAML comments.

## Non-Goals For First Pass

- Replacing the Sonoff S31 docs/protocol names globally.
- Making Bluetooth Proxy a required feature for all ESP32-C3 builds.
- Supporting ESP-NOW as the first bring-up transport.
- Multi-outlet or multi-relay support.
- Using energy readings as halachic decision input.
