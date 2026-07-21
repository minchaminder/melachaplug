# Calendar Timer Schedules (Wall Switch / Melacha Plug)

## Goal

Give users a **timer** on Melacha-aware hardware (including the Athom US Key Switch / SW13-ESP-1US) that works two ways:

1. **Clock timer** — ordinary on/off schedules (daily, weekly, one-shot dates, etc.).
2. **Calendar / zmanim timer** — the same on/off actions, but each edge is anchored to a Jewish calendar day and a zman (with a signed minute offset), not a fixed clock time.

One product mental model: *schedules*. Some schedules use wall-clock anchors; some use zmanim anchors. Both can coexist on the same device.

This is **appliance automation**, not a replacement for Melacha Plug’s existing Shabbos/Yom Tov lockout. Shabbos mode can still disable the physical button and override relay policy when melacha is forbidden. Calendar timers are for “turn the urn / light / heater on and off around zmanim,” including cases that intentionally run *into* Shabbos or *after* midnight.

---

## Target hardware

Primary example: **Athom US Key Switch Made For ESPHome** (SKU `SW13-ESP-1US`).

Parallel plug target: **Athom ESP32-C3 US Plug Made For ESPHome** (SKU
`PG03V3-US16A-ESP-1`) is tracked separately in
[`ATHOM_ESP32C3_US_PLUG_PLAN.md`](./ATHOM_ESP32C3_US_PLUG_PLAN.md).

| Item | Value |
| --- | --- |
| Form factor | US single-gang wall switch |
| MCU (from 31 Oct 2025) | ESP32-C3 (4MB) |
| Load | Relay, up to 1200W |
| Control | Button + automation / schedules |
| Optional local radio | ESP-NOW-capable on ESP32-C3 builds |
| Official stock YAML | [`athom-1gang-switch.yaml`](https://github.com/athom-tech/esp32-configs/blob/main/athom-1gang-switch.yaml) |

Older units of the same SKU used ESP8285. Calendar timers assume a Melacha calendar engine on-device (or on a paired brain such as CoreInk). ESP32-C3 is the preferred target for richer schedule UI and local zmanim.

ESP-NOW is a viable optional transport on the ESP32-C3 version. It is not assumed
to be enabled by the stock product YAML, and it must not change relay semantics:
ESP-NOW relay commands go through the same guarded command path as web, API, and
physical-button actions. Older ESP8285 units are not the target for this
ESP-NOW path.

---

## What the user can set

Each **schedule** has:

| Field | Meaning |
| --- | --- |
| Name | Human label (“Friday lights”, “Shabbos urn”) |
| Enabled | On/off without deleting |
| Actions | Ordered list of **edges**: turn **ON** or **OFF** at an anchor |
| Repeat | How often the schedule applies (see below) |
| Priority | Optional; used when two schedules disagree |

Each **edge** has:

| Field | Clock timer | Calendar / zmanim timer |
| --- | --- | --- |
| Action | `ON` or `OFF` | `ON` or `OFF` |
| Anchor | Fixed local time (`HH:MM`) | A **zman** (e.g. shkiah, chatzos, Rabbeinu Tam) |
| Offset | Optional ± minutes from that clock time | Signed minutes before (−) or after (+) the zman |
| Day filter | Weekdays / dates | Civil weekday, Hebrew day type, or both |

Offsets may cross midnight. A “turn off 4 hours after Friday shkiah” edge that lands at 12:40 AM Saturday is still **one Friday-shkiah-based edge**, not a separate Saturday rule.

---

## Repeat modes

Same repeat UI for both clock and calendar schedules:

| Mode | Behavior |
| --- | --- |
| Once | Specific civil date(s), or one Hebrew date |
| Daily | Every day |
| Weekly | Selected weekdays (Sun–Sat civil, or erev Shabbos / Shabbos helpers) |
| Hebrew pattern | e.g. every erev Shabbos, every Shabbos, every Yom Tov eve, weekdays only |
| Custom date list | Explicit list of civil or Hebrew dates |

Calendar schedules resolve zmanim **for the day the edge is attached to**. If an offset crosses into the next civil day, execution still happens at that absolute timestamp.

---

## Worked example (user scenario)

Desired behavior:

1. **Friday** — turn **ON** 15 minutes **before** shkiah.
2. **Friday** — turn **OFF** 4 hours **after** shkiah (even if that is after 12:00 AM Saturday).
3. **Saturday** — turn **ON** 1 hour **after** chatzos.
4. **Saturday** — turn **OFF** 1 hour **after** Rabbeinu Tam (havdalah / 72 minutes after shkiah, per device minhag).

### As one schedule: “Shabbos lighting / urn”

| # | Day attachment | Action | Anchor | Offset |
| --- | --- | --- | --- | --- |
| 1 | Erev Shabbos (Friday) | ON | Shkiah | −15 min |
| 2 | Erev Shabbos (Friday) | OFF | Shkiah | +240 min |
| 3 | Shabbos (Saturday) | ON | Chatzos | +60 min |
| 4 | Shabbos (Saturday) | OFF | Rabbeinu Tam | +60 min |

### Example resolution (illustrative times)

Assume one Friday in Brooklyn:

| Zman | Clock (example) |
| --- | --- |
| Friday shkiah | 8:00 PM |
| Saturday chatzos | 1:05 PM |
| Saturday Rabbeinu Tam | 9:12 PM |

Resolved transitions:

| Edge | Resolved local time | Relay |
| --- | --- | --- |
| Fri shkiah − 15 | Friday 7:45 PM | ON |
| Fri shkiah + 240 | **Saturday 12:00 AM** | OFF |
| Sat chatzos + 60 | Saturday 2:05 PM | ON |
| Sat RT + 60 | Saturday 10:12 PM | OFF |

The second edge is still a **Friday shkiah** rule. Crossing midnight does not move it onto Saturday’s calendar filters.

---

## Clock-timer examples (same product)

These use the same schedule object; only the anchor type changes.

| Schedule | Repeat | Edges |
| --- | --- | --- |
| Porch light | Daily | ON 6:00 PM, OFF 11:00 PM |
| Weekday heater | Mon–Thu | ON 5:30 AM, OFF 8:00 AM |
| Guest mode | Once: 2026-07-24 | ON 4:00 PM, OFF next day 10:00 AM |
| Motzei Shabbos clock | Weekly Sat | ON 10:30 PM (fixed; not zman-linked) |

Users can mix: one schedule clock-based, another zman-based, on the same switch.

---

## Supported zman anchors (v1 target)

Minimum set for the wall-switch timer:

| Anchor ID | Meaning | Notes |
| --- | --- | --- |
| `alot` | Dawn | Optional v1 |
| `hanetz` | Sunrise | |
| `sof_zman_shema` | Latest Shema | Optional |
| `chatzos` | Midday | Required for the Saturday example |
| `mincha_gedola` | | Optional |
| `plag` | Plag hamincha | Already used for Early Shabbos |
| `shkiah` | Sunset (−0.833° or product default) | Required |
| `tzeit` | Nightfall (degree or minutes mode) | |
| `rabbeinu_tam` | Havdalah / 72 after shkiah (or degree equivalent) | Required; follow `ZMANIM_MODE_PLAN.md` |
| `candle_lighting` | Shkiah − candle minutes | Convenience alias |

Offsets are always **signed minutes** relative to the resolved zman timestamp.

Degree vs minutes minhag for how `tzeit` / `rabbeinu_tam` are computed follows [`ZMANIM_MODE_PLAN.md`](./ZMANIM_MODE_PLAN.md). The timer UI should not invent a third minhag path.

---

## Midnight and day-boundary rules

1. **Edge day** = the calendar day the rule is attached to (erev Shabbos, Shabbos, Monday, …).
2. **Execution time** = `zman(day) + offset` as an absolute UTC/local timestamp.
3. If execution falls after 12:00 AM civil next day, still execute then. Do **not** re-filter against the next day’s weekday/Hebrew rules.
4. Hebrew day boundaries for *which* edges apply still use Melacha’s existing sunset / early-Shabbos logic when the filter is “erev Shabbos” vs “Shabbos,” not civil midnight alone.
5. Missed transitions (power loss, late time sync): apply only the **latest effective** desired state for “now,” then arm the next future edge (same recover rule as CoreInk/S31 schedules).

---

## Interaction with Melacha Shabbos mode

| Layer | Job |
| --- | --- |
| Calendar timer schedules | Decide desired ON/OFF from user rules |
| Shabbos / Yom Tov mode | Melacha safety: button lock, optional force OFF / inverted mode, LED flash |

Recommended product defaults:

- Schedules may request ON during Shabbos (urn / lights that are meant to run).
- Physical button remains locked while `shabbos_mode` is on (existing Melacha behavior).
- Optional per-schedule flag: **Respect melacha lockout** — if on, schedule edges that would turn the relay ON while melacha is forbidden are skipped (or forced OFF). Default **off** for “urn / timer” use cases; **on** for “don’t accidentally heat water on Shabbos” use cases.
- Override Shabbos Mode (existing switch) still wins for manual recovery.

Manual and live command handling for wall-switch builds:

- On weekdays, a physical wall-button press is a user command. It should toggle
  the relay immediately and remain the current manual state until the next
  explicit schedule edge, protected-period transition, or user command. The
  periodic Melacha check must not blindly force the weekday relay state every
  minute.
- During Shabbos / Yom Tov, live relay commands are ignored or rejected from all
  interactive paths: physical button, web UI, Home Assistant/API, and ESP-NOW.
  The device may still execute preconfigured schedule/policy edges if the
  product setting allows them; those are scheduled policy actions, not live user
  commands.
- All relay command sources must use the same guarded relay setter/script. The
  raw relay GPIO switch must not be exposed as an unguarded writable control.
- Exiting Shabbos / Yom Tov unlocks live commands, but must not replay button,
  API, web, or ESP-NOW commands that arrived while locked.

---

## Conflict resolution

When two enabled schedules disagree at the same moment:

1. Higher **priority** wins.
2. Else the edge with the **later** `effective_at` wins (last writer).
3. Else prefer **OFF** (safe default for a wall switch).

UI should show “next transition” and “current winning schedule” so conflicts are visible.

---

## UI sketch (web / CoreInk)

```text
Schedules
  [+] Add schedule

  Schedule: "Shabbos lights"
    Repeat: Weekly → Erev Shabbos + Shabbos
    Mode: Calendar (zmanim)

    Edges:
      1. Fri / Erev Shabbos   ON    Shkiah − 15 min
      2. Fri / Erev Shabbos   OFF   Shkiah + 4 hr
      3. Sat / Shabbos        ON    Chatzos + 1 hr
      4. Sat / Shabbos        OFF   Rabbeinu Tam + 1 hr

    Next: Fri 7:45 PM → ON
    Then: Sat 12:00 AM → OFF
```

Clock schedules use the same list; the anchor picker shows times instead of zmanim.

---

## Data model (implementation sketch)

Logical rule (before compiling to absolute transitions):

```json
{
  "schedule_id": "shabbos-lights",
  "name": "Shabbos lights",
  "enabled": true,
  "priority": 10,
  "respect_melacha_lockout": false,
  "repeat": {
    "type": "weekly_hebrew",
    "days": ["erev_shabbos", "shabbos"]
  },
  "edges": [
    { "on_day": "erev_shabbos", "action": "ON",  "anchor": { "type": "zman", "id": "shkiah" },       "offset_minutes": -15 },
    { "on_day": "erev_shabbos", "action": "OFF", "anchor": { "type": "zman", "id": "shkiah" },       "offset_minutes": 240 },
    { "on_day": "shabbos",      "action": "ON",  "anchor": { "type": "zman", "id": "chatzos" },      "offset_minutes": 60 },
    { "on_day": "shabbos",      "action": "OFF", "anchor": { "type": "zman", "id": "rabbeinu_tam" }, "offset_minutes": 60 }
  ]
}
```

Clock edge variant:

```json
{ "on_day": "daily", "action": "ON", "anchor": { "type": "clock", "time_local": "18:00" }, "offset_minutes": 0 }
```

On-device (or CoreInk Profile A), rules compile into a rolling list of absolute UTC transitions — the same shape already used in [`COREINK_S31_JSON_SCHEMAS.md`](./COREINK_S31_JSON_SCHEMAS.md) (`transitions[].at_utc` + `desired_relay_state`).

---

## Architecture fit

| Deployment | Who resolves zmanim → UTC | Who flips the relay |
| --- | --- | --- |
| Standalone Melacha wall switch (ESP32-C3) | On-device calendar engine | Local relay |
| CoreInk + satellite (Profile A) | CoreInk compiles transitions | Switch/plug executes cache |
| CoreInk + satellite (Profile B) | Satellite calculates from settings | Local relay |

Standalone Athom US Key Switch is the simplest user story for this doc: configure schedules on the device web UI, run locally with Melacha zmanim.

Wi-Fi / HTTP remains the baseline transport. ESP-NOW can be added for
ESP32-C3-to-ESP32-C3 local command/status messages, but it is only a transport:
pairing, channel management, acknowledgements, and retries do not replace the
local relay guard or Shabbos/Yom Tov lockout rules above.

Related docs:

- [`PARALLEL_DEVICE_ARCHITECTURE_PLAN.md`](./PARALLEL_DEVICE_ARCHITECTURE_PLAN.md) — shared core, Shabbos Timer, hardware targets, and build matrix
- [`ZMANIM_MODE_PLAN.md`](./ZMANIM_MODE_PLAN.md) — how shkiah / RT / tzeit are computed
- [`ATHOM_ESP32C3_US_PLUG_PLAN.md`](./ATHOM_ESP32C3_US_PLUG_PLAN.md) — PG03V3-US16A ESP32-C3 plug support plan
- [`COREINK_OFFLINE_PRODUCT_PLAN.md`](./COREINK_OFFLINE_PRODUCT_PLAN.md) — brain + relay split
- [`COREINK_S31_PROTOCOL.md`](./COREINK_S31_PROTOCOL.md) — compiled transition packets

---

## Non-goals (v1)

- Cloud calendars or Google/Outlook sync
- Recurrence more complex than weekly + Hebrew day-types + date lists
- Multi-gang independent schedules beyond one relay per schedule set (2/3-gang can wait)
- Halachic psak engine beyond existing Melacha minhag switches

---

## Acceptance examples

- [ ] User can create a daily clock ON/OFF schedule and see next fire times.
- [ ] User can create the four-edge Friday/Saturday zmanim example above.
- [ ] Friday shkiah + 4h that lands after midnight executes after 12:00 AM and does not require a Saturday filter.
- [ ] Saturday chatzos + 1h and Rabbeinu Tam + 1h resolve with current location + zmanim mode.
- [ ] Power loss mid-schedule: after time is valid again, relay matches latest due edge, not a replay of every missed edge.
- [ ] With `respect_melacha_lockout: true`, ON edges during melacha-forbidden windows are suppressed; with `false`, they run (button still locked by Shabbos mode).
- [ ] Weekday wall-button toggles are not overwritten by the next periodic Melacha check; they persist until a schedule edge, protected-period transition, or later user command.
- [ ] During Shabbos / Yom Tov, physical button, web, API, Home Assistant, and ESP-NOW live relay commands cannot change the relay.

---

## Summary

The wall switch timer is a **unified schedule system**: clock anchors *or* zmanim anchors, same repeats and ON/OFF edges. Jewish calendar attachment means “fire relative to shkiah / chatzos / Rabbeinu Tam on this kind of day,” including offsets that cross civil midnight — so Friday lights can turn off at 12:40 AM Saturday without pretending that edge belongs to Saturday’s rules.
