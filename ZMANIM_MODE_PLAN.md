# Plan: Degree Mode vs Minutes Mode for Zmanim

## Goal

Let users choose how Melacha Plug calculates Shabbos start, Shabbos end, and tzeit:

1. **Degree mode** — sun elevation (current default / Alter Rebbe)
2. **Minutes mode** — fixed minutes before/after shkiah

Ship defaults stay in YAML so devices boot ready. Users can change mode and values in the web UI without reflashing.

---

## V1 decisions (locked)

| Topic | Decision |
|---|---|
| Ship default mode | **Degree** (Alter Rebbe-style, backward compatible) |
| Degree mode offsets | **Keep** existing `min_offset_start` / `min_offset_end` (elevation + offset) |
| Minutes UI values | **Positive** “before” / “after” fields; code applies sign |
| Field layout | **Separate** Degree fields vs Minutes fields (no overloaded semantics) |
| Tzeit | **Display-only** (does not drive relay / melacha) |
| Shared calc | **Required** — one boundary helper for melacha, UI, random-minute, next-Shabbos |
| Early Shabbos | **Keep current behavior**: plag base, then Degree-mode start offset still applied |
| Plag helper | **Dated**: `getPlagTime(ESPTime date)` (not today-only) |
| Early Shabbos API | Explicit `honor_early_shabbos` on boundary helper (callers choose) |
| Build validation | Compile ≥1 ESP8266 target and ≥1 ESP32 target before merge |

### Early Shabbos (documented intentional)

Today (`updateHdate`): if Early Shabbos is on and it is a weekday, start boundary = `getPlagTime()`, then **`min_offset_start` is still applied**.

V1 keeps that as: **plag + Degree-mode start offset**.

Minutes mode + Early Shabbos: still use plag as the start base; do **not** also subtract `min_before_shkiah_start` (plag replaces the shkiah−N path). Degree-mode start offset continues to apply after plag for backward compatibility. If that proves confusing later, change it deliberately — not silently in v1.

**Call-site policy (v1):** `getBoundaryTimestamp(..., honor_early_shabbos=true)` for melacha START, next-Shabbos START display, and random-minute START window so UI/relay match the live Early Shabbos switch. Pass `false` only when a caller intentionally wants the non-plag start (not needed for default consumers).

---

## Current behavior

| Setting | YAML / UI today | Role |
|---|---|---|
| `deg_shabbos_starts` | `-0.833` (shkiah) | Elevation used for Shabbos **start** |
| `deg_shabbos_ends` | `-8.5` (Alter Rebbe) | Elevation used for Shabbos **end** |
| `deg_tzeit` | `-6` | Elevation for **tzeit display only** |
| `min_offset_start` | `-30` | Signed minutes on top of start elevation (legacy) |
| `min_offset_end` | `0` | Signed minutes on top of end elevation (legacy) |

Logic lives mainly in `esphome/header-files/melachaplug_main.h` (`updateHdate`): pick elevation + offset → `sun.sunset(elevation)` → apply `offset * 60` seconds (see ~lines 75–100).

There is **no** first-class “N minutes after shkiah” end/tzeit mode.

Tzeit is shown via `ts_tzeit` but does **not** drive relay / melacha mode. Shabbos end does.

### Known bugs / gaps to fix in this work

1. **`deg_shabbos_ends` range** — `min_value: -5` but default is `-8.5` (`melacha_config.yaml`). Widen min (e.g. `-20` or `-30`) so Alter Rebbe and Rabbeinu Tam–style degrees fit.
2. **Random-minute / next-Shabbos** — degree-only paths; in this tree they reference IDs/substitutions that do not appear defined (`deg_shabbos_start_global` / `deg_shabbos_end_global`, `${deg_shabbos_start}`). Treating these plugins as **required** means fixing those wires as part of this change, not leaving them optional/broken.
3. **`ts_tzeit`** — ESPHome `sun` text sensor uses compile-time `${deg_tzeit}`. Runtime-configurable tzeit (degree and/or minutes) needs a **template** sensor instead.

---

## Proposed UX

### Top-level control

- **Zmanim Mode** (`select`): `Degree` | `Minutes`
- YAML default: `zmanim_mode: "Degree"`
- Persist across reboot (`restore_value`)

### Degree mode

- Elevations: start / end / tzeit (tzeit display)
- Legacy signed offsets: `min_offset_start`, `min_offset_end` on top of those elevations
- **Set To Alter Rebbe Zmanim** → set Degree defaults, force mode to `Degree`, refresh checks

### Minutes mode

Always base on **shkiah** = sunset at `-0.833°`. UI values are **positive**:

| Control | YAML key | Meaning | Code |
|---|---|---|---|
| Minutes Before Shkiah (Start) | `min_before_shkiah_start` | How early Shabbos starts | `shkiah - value * 60` |
| Minutes After Shkiah (End) | `min_after_shkiah_end` | When melacha / Shabbos mode ends | `shkiah + value * 60` |
| Minutes After Shkiah (Tzeit) | `min_after_shkiah_tzeit` | Nightfall **display** | `shkiah + value * 60` |

Do **not** reuse negative UI values for “before.” Negatives remain only on legacy Degree offsets (`min_offset_*`).

Example formulas:

```text
shkiah     = sun.sunset(date, -0.833°)
start_time = shkiah - min_before_shkiah_start * 60
end_time   = shkiah + min_after_shkiah_end * 60
tzeit_time = shkiah + min_after_shkiah_tzeit * 60
```

---

## Web UI entities

Separate fields (recommended / locked for v1):

| Entity | Type | Used when |
|---|---|---|
| Zmanim Mode | `select` | Always |
| Minute Offset Start | `number` (signed) | Degree |
| Minute Offset End | `number` (signed) | Degree |
| Degree Shabbos Starts | `number` | Degree |
| Degree Shabbos Ends | `number` | Degree |
| Degree Tzeit | `number` | Degree (display) |
| Minutes Before Shkiah (Start) | `number` (positive) | Minutes |
| Minutes After Shkiah (End) | `number` (positive) | Minutes |
| Minutes After Shkiah (Tzeit) | `number` (positive) | Minutes (display) |
| Set To Alter Rebbe Zmanim | `button` | Forces Degree + Alter Rebbe defaults |

Custom web assets can later hide inactive-mode fields; not required for v1 if names are clear.

### Suggested ranges

| Field | Min | Max | Notes |
|---|---|---|---|
| `deg_shabbos_starts` | ~`-5` | `30` | Keep early-Shabbos headroom |
| `deg_shabbos_ends` | ~`-20` or `-30` | `30` | Must allow `-8.5` (fix current `-5` floor) |
| `deg_tzeit` | ~`-20` | `0` | Display |
| `min_offset_start` | `-300` | `120` | Legacy signed |
| `min_offset_end` | `-120` | `120` | Legacy signed |
| `min_before_shkiah_start` | `0` | `300` | Positive UI |
| `min_after_shkiah_end` | `0` | `120` | Fits 72 RT |
| `min_after_shkiah_tzeit` | `0` | `120` | Display |

---

## YAML ship defaults

On each device YAML (`melachaplug-*.yaml`):

```yaml
substitutions:
  zmanim_mode: "Degree"

  # Degree mode (Alter Rebbe-style ship default)
  deg_shabbos_starts: "-0.833"
  deg_shabbos_ends:   "-8.5"
  deg_tzeit:          "-6"
  min_offset_start:   "-30"    # legacy signed; Degree fine-tune
  min_offset_end:     "0"

  # Minutes mode (positive UI values; used when mode = Minutes)
  min_before_shkiah_start: "30"   # code: shkiah - 30min
  min_after_shkiah_end:    "42"
  min_after_shkiah_tzeit:  "42"
```

Exact Minutes-mode defaults are minhag product choices; `42` is a placeholder until chosen. Ship mode stays **Degree**.

---

## Shared boundary helper (required)

Centralize in `melachaplug_main.h` (or shared header included by plugins).

**Not** just `is_end` for “today.” Next-Shabbos needs **arbitrary dates**.

### Dated plag

Refactor today’s `getPlagTime()` (uses `sntp_time.now()` only) to:

```text
time_t getPlagTime(ESPTime date);
```

Compute hanetz/shkiah amiti and plag for the civil day of `date`, so next-Shabbos START can use Early Shabbos plag on a **future** Friday (or other start day), not only “today.”

Keep a thin `getPlagTime()` wrapper that calls `getPlagTime(id(sntp_time).now())` if useful for existing call sites.

### Boundary helper signature

Suggested shape:

```text
enum BoundaryType { START, END, TZEIT };

time_t getBoundaryTimestamp(
    ESPTime date,
    BoundaryType type,
    bool honor_early_shabbos
);
```

**Why `honor_early_shabbos`:** do not silently bake “always honor the live Early Shabbos switch” into every START, including future next-Shabbos, without the caller saying so. V1 default consumers pass `true` so display and relay match the switch; the parameter makes that explicit and testable.

Behavior:

1. Normalize `date` to local civil day as needed for sun calc.
2. If `type == START` and `honor_early_shabbos` and Early Shabbos switch is on: base = `getPlagTime(date)`; then apply Degree `min_offset_start` (v1 intentional). Do **not** also apply Minutes `min_before_shkiah_start`.
3. Else if mode == Minutes:
   - `START` → `sunset(date, -0.833) - min_before_shkiah_start * 60`
   - `END`   → `sunset(date, -0.833) + min_after_shkiah_end * 60`
   - `TZEIT` → `sunset(date, -0.833) + min_after_shkiah_tzeit * 60`
4. Else Degree:
   - `START` → `sunset(date, deg_shabbos_starts) + min_offset_start * 60`
   - `END`   → `sunset(date, deg_shabbos_ends) + min_offset_end * 60`
   - `TZEIT` → `sunset(date, deg_tzeit)` (v1 = elevation only for Degree tzeit)

`END` / `TZEIT` ignore Early Shabbos / plag.

Consumers (all required to use this helper):

| Consumer | Use |
|---|---|
| `updateHdate` / melacha relay logic | Today’s START or END; START with `honor_early_shabbos=true` |
| `todays_melacha_check_time` | Same timestamp |
| Tzeit template sensor | `TZEIT` for today (and refresh on change) |
| `next_shabbbos_times` | START/END for **future** dates; START with `honor_early_shabbos=true` |
| `random_minute_generator` | START/END window; START with `honor_early_shabbos=true` |

Also fix plugin wiring so substitutions/globals match the main package (replace undefined `deg_shabbos_start_global` / `${deg_shabbos_start}` usage with the helper, not parallel degree-only paths).

---

## Calculation / UI changes by area

### `MelachaPlug::updateHdate`

Become a thin wrapper: choose START vs END from “currently assur?” then `getBoundaryTimestamp(now, type, honor_early_shabbos=true)` for START, then existing hebrew-day advance + publish check time.

### Tzeit display

- **Replace** sun-platform `ts_tzeit` with a **template** text sensor.
- Update whenever mode, location, time sync, or tzeit-related numbers change.
- Degree: format from elevation-based boundary; Minutes: shkiah + positive minutes.

### `setAlterRebbeZmanim`

- Mode → `Degree`
- Degrees → `-0.833` / `-8.5` / tzeit default
- Optionally reset Degree offsets to YAML defaults (`-30` / `0`)
- `runMelachaChecks()` + refresh template times

### Random-minute plugin

- Required in scope
- On Shabbos start / window generation: use helper START/END timestamps, not sun `on_sunset` elevation-only hooks that ignore Minutes mode
- May still use an interval or a computed trigger; must not assume degree-only globals

### Next-Shabbos plugin

- Required in scope
- Call helper with **future** `ESPTime` + `START` / `END` and `honor_early_shabbos=true` for START
- Relies on dated `getPlagTime(date)` when Early Shabbos is on
- Drop reliance on undefined degree globals

---

## Why not map “72 minutes” to one degree?

Sun depression for a fixed clock offset varies by latitude and season. At default Brooklyn coords (~40.67°N), 72 minutes after shkiah is roughly **−11.5°** in summer to **~−14.4°** near equinox. Israel-oriented **−16.1°** is a degree stand-in, not fixed 72 minutes in NY.

Minutes mode is the correct way to honor “72 minutes after shkiah” exactly.

---

## Implementation sketch (files)

| Area | Touch |
|---|---|
| Mode + numbers + tzeit template | `esphome/plugins/melachaplug/melacha_config.yaml` |
| Boundary helper, dated plag, `updateHdate` / Alter Rebbe | `esphome/header-files/melachaplug_main.h` |
| Device defaults | `esphome/melachaplug-*.yaml` |
| Next Shabbos (required) | `esphome/plugins/next_shabbbos_times/*` |
| Random minute (required) | `esphome/plugins/random_minute_generator/*` |
| Optional UI polish | `esphome/web_assets/*` |

No hardware / RTC changes. Still needs SNTP (or future RTC) for valid wall clock before zmanim run.

---

## Acceptance criteria

- [ ] Web UI switches **Degree** ↔ **Minutes**; persists across reboot
- [ ] YAML substitutions define ship-ready defaults; default mode is **Degree**
- [ ] Degree mode = elevation + existing signed `min_offset_*` (backward compatible)
- [ ] Minutes mode: start = `shkiah − N` with **positive** N; end/tzeit = `shkiah + N` with positive N
- [ ] Changing mode or values re-runs melacha checks and refreshes displayed times
- [ ] Early Shabbos: plag + Degree start offset (documented intentional); callers pass `honor_early_shabbos` explicitly
- [ ] `getPlagTime(ESPTime date)` works for future dates (next-Shabbos Early Shabbos START)
- [ ] `ts_tzeit` is a template sensor; runtime Degree/Minutes tzeit works
- [ ] `deg_shabbos_ends` min_value allows `-8.5` (and similar)
- [ ] Next-Shabbos and random-minute use **shared** `getBoundaryTimestamp(date, type, honor_early_shabbos)` and work in both modes
- [ ] Alter Rebbe button restores Degree defaults and mode
- [ ] Tzeit remains display-only
- [ ] **Build validation:** `esphome compile` succeeds for at least one **ESP8266** target (e.g. `melachaplug-s31.yaml` or `melachaplug-8285.yaml`) and one **ESP32** target (e.g. `melachaplug-esp32.yaml`). Text review alone is not enough — this change touches YAML, generated IDs, template select/number, and C++ lambdas.

---

## Non-goals (this plan)

- Onboard RTC / time without network
- Replacing the Jewish calendar engine
- Ending melacha at tzeit (display-only stays)
- Forcing one minhag for all users
- Custom web UI hide/show of inactive fields (nice-to-have)

---

## Summary

Add **Zmanim Mode**. Ship **Degree** with legacy elevation + signed offsets. **Minutes** uses separate positive before/after fields (`shkiah ± N`). One dated boundary helper (`getBoundaryTimestamp` + dated `getPlagTime`, with explicit `honor_early_shabbos`) feeds melacha, tzeit display, random-minute, and next-Shabbos. Fix degree ranges and replace compile-time `ts_tzeit` with a template sensor. Validate with ESP8266 + ESP32 compiles before merge.
