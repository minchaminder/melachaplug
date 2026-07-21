# Online-Ready, Offline-Friendly Support

This note documents the current challenges for Melacha Plug preflashed devices
used in homes with limited or no internet access.
The goal is to separate true runtime requirements from documentation/build
convenience, and to define the **online-ready, offline-friendly** support plan.

Core concept:

- Keep the standard firmware online-ready.
- Keep the current fail-safe boot behavior.
- Add offline-friendly behavior to the preflashed firmware for homes where the
  device can join WiFi or run its fallback access point, but cannot rely on WAN
  internet.
- Make the local web UI load without internet so configuration is still
  possible.
- Keep online conveniences, such as IP geolocation, but make their failure
  non-blocking.

## Scope

"Offline" can mean a few different things:

- The home has WiFi/LAN, but no WAN internet.
- The home blocks outbound HTTP/DNS/NTP traffic.
- The device is used through its local web UI or fallback access point only.
- Firmware is not compiled on site; the device only uses the preflashed binary.

Melacha Plug's core calendar logic is local once the device has the correct
time, timezone, latitude, longitude, and configuration. The main offline risk is
not the Jewish calendar code itself. The risk is how the device gets the
supporting data it needs.

## What Works Locally Today

- The Hebrew calendar and melacha checks run on the device.
- The sun/zmanim calculations run on the device.
- The local ESPHome web server endpoint is available on the LAN. The full UI is
  served without WAN internet by embedding the ESPHome web UI asset in firmware.
- The fallback captive portal can be used when the device cannot join WiFi.
- Latitude, longitude, offsets, Eretz Yisrael mode, inverted Shabbos mode, and
  other template settings can be saved on the device.
- OTA updates are local-network capable when the user has a firmware file and
  local network access.

## CoreInk Console Alternative

The current S31-local calculation model aligns with the maintainer-first
CoreInk approach:

- Keep the full Melacha Plug calendar/zmanim brain on every S31.
- Use CoreInk as the house clock, setup console, status display, and settings
  distributor.
- Let each S31 calculate its own current policy and next transition after it has
  valid time and a valid settings snapshot.
- Have each S31 report its calculated policy, next transition, settings
  generation, and calendar-engine version back to CoreInk.
- Have CoreInk display disagreement instead of silently choosing one plug as
  correct.

This is a good offline fit because, after initialization, a powered S31 can keep
operating even if CoreInk reboots or disappears, subject to its local time
uncertainty and settings-expiry policy.

The cost is firmware discipline. Every S31 must carry the same tested calendar
engine, support the same settings schema, and reject stale or unauthenticated
settings. CoreInk must track versions and surface mismatches.

ESP-NOW may be used for the CoreInk-to-S31 control link in this profile because
time/settings/status messages can be kept small. It should not be described as
mesh; any forwarding or repeating must be implemented separately and tested.

## Current Offline Challenges

### 1. Web UI Assets Must Be Served Locally

The firmware enables the ESPHome web server with:

```yaml
web_server:
  port: 80
  include_internal: true
  version: 2
  js_include: web_assets/www-v2.js
  js_url: ""
  css_url: ""
```

The default ESPHome web server configuration references JavaScript and CSS
assets from ESPHome-hosted URLs. In an offline home, the device can still answer
on port 80, but the page does not render correctly when the browser cannot fetch
those external assets.

The firmware avoids that failure by embedding the ESPHome version 2 web UI asset
in the image and disabling external asset URLs.

How offline-friendly web UI loading works:

- The device serves the ESPHome web page from its own local IP address or
  fallback access point.
- The firmware embeds the matching ESPHome web UI JavaScript asset at build time
  with `js_include`.
- The generated page references the embedded device-local asset instead of an
  internet URL.
- `js_url: ""` disables the external JavaScript URL.
- `css_url: ""` disables the external stylesheet URL.
- The firmware uses `version: 2`; ESPHome web server version 2 carries the UI
  styling in the JavaScript bundle, so this does not need a separate
  `css_include`.

Implementation:

- Keep the standard firmware online-ready.
- Embed `esphome/web_assets/www-v2.js` with `js_include`.
- Set `js_url: ""` and `css_url: ""` so the generated page does not point the
  browser at external asset URLs.
- Explicitly set `version: 2` so the bundled asset file matches the generated
  HTML.
- Validate firmware size and stability on ESP8266 as part of the firmware
  release process.
- For 1 MB ESP8266 S31 devices already running the older firmware, use the
  S31 OTA bridge before installing the larger offline-friendly image.

Example offline web server shape:

```yaml
web_server:
  port: 80
  include_internal: true
  version: 2
  js_include: web_assets/www-v2.js
  js_url: ""
  css_url: ""
```

The exact version 2 asset file is pinned in `esphome/web_assets/www-v2.js` and
stored in the release bundle so the firmware can be rebuilt without network
access.

### 1a. S31 OTA Size Constraint

The Sonoff S31 profile uses a 1 MB ESP8266 flash layout. Embedding the local web
UI asset makes the offline-friendly firmware larger than the OTA slot available
from the older S31 firmware.

Observed failure:

```text
[E] [ota.arduino_esp8266:058] Write error: 4
[E] [web_server.ota:166] OTA write failed: 131
```

`Write error: 4` is `UPDATE_ERROR_SPACE` from the ESP8266 Arduino updater. The
uploaded firmware is valid, but the currently running image cannot reserve
enough OTA flash space for it.

Current S31 release files:

```text
bins/global-sonoff-s31-ota-bridge.bin              350576 bytes
bins/global-sonoff-s31-offline-friendly-ota.bin    653184 bytes
```

Release size budget:

```text
old S31 OTA max after global-sonoff-s31.bin:        413696 bytes
bridge OTA max after global-sonoff-s31-ota-bridge:  671744 bytes
full offline-friendly image size:                   653184 bytes
required bridge headroom reserve:                    16384 bytes
actual bridge headroom:                              18560 bytes
```

The release must include both files and must pass:

```bash
scripts/check-s31-ota-room.sh
```

This check enforces both OTA hops:

- The currently shipped S31 firmware must have enough OTA room to accept the
  bridge.
- The installed bridge must leave enough OTA room for the full offline-friendly
  image, plus the release reserve.

The bridge image is intentionally temporary and minimal:

- It starts the access point `Melacha S31 OTA Bridge`.
- It serves a local ESPHome web server v1 page at `http://192.168.4.1/`.
- The page includes the firmware file input and `Update` button.
- It does not use external JavaScript or CSS.
- It disables mDNS to keep the image small enough.
- It keeps relay GPIO12 in `ALWAYS_OFF`.
- It is not the normal Melacha Plug runtime firmware.

Required OTA migration for existing S31 units:

1. From the current S31 web UI, upload
   `bins/global-sonoff-s31-ota-bridge.bin`.
2. Wait for the device to reboot.
3. Connect to WiFi network `Melacha S31 OTA Bridge`.
4. Open `http://192.168.4.1/`.
5. Use the OTA Update file input to upload
   `bins/global-sonoff-s31-offline-friendly-ota.bin`.
6. Wait for the device to reboot into the normal Melacha Plug firmware.
7. Configure WiFi, location, and any local time source from the normal local web
   UI.

Preflashed or serial-flashed S31 units do not need the bridge. They should be
flashed directly with the offline-friendly S31 firmware.

### 2. Time Sync Requires a Reachable Time Source

Current firmware uses ESPHome SNTP:

- `esphome/plugins/melachaplug/melacha_config.yaml`
- `time: platform: sntp`

The device sets `time_valid` only after time sync. The melacha checks return
early until time is valid. On boot, the firmware also turns Shabbos Mode on as a
fail-safe. If a device reboots in a home with no reachable NTP source, it may
stay in that fail-safe state because it does not know the current date and time.

This is the main runtime dependency after the local setup page can load.

Router/gateway note:

Many home routers use NTP/SNTP for their own clock, but that does not always
mean they provide an NTP server for LAN clients. Some routers only act as NTP
clients. Some can advertise or provide a local NTP service. The offline-friendly
firmware must not assume either case. It must boot into the existing
fail-safe behavior, load the local web UI, allow entry of an NTP server IP, and
try the LAN gateway/router IP as an automatic first candidate.

Plan:

- Try the LAN gateway/router IP as the first NTP/SNTP candidate.
- Add a local web UI field for an explicit NTP server IP or hostname.
- Persist the configured NTP server across reboots.
- Fall back to configured public NTP servers when internet is available.
- Keep the device bootable and configurable even when no NTP source is reachable.

### 3. Timezone Is Compile-Time Today

The YAML files use:

```yaml
timezone: America/New_York
```

For preflashed units, this means the timezone is effectively locked into the
firmware image. A device outside that timezone can update latitude and longitude
from the web UI, but the timezone still needs to match the actual location for
correct local dates and displayed times.

Plan:

- Document the timezone limitation for preflashed devices.
- Keep timezone as a compile-time firmware setting in the current scope.
- Document that changing latitude/longitude alone does not fully relocate a
  preflashed unit if the timezone is wrong.

Technical requirement:

Each preflashed firmware image has an explicit timezone scope.

### 4. Auto Location Uses an Internet API

The current location helper calls:

```cpp
http://ip-api.com/json/?fields=status,regionName,city,zip,lat,lon
```

This only works when the device has internet access. In an offline home, the API
call fails. This is not required for the calendar logic if the device already
has correct latitude and longitude.

Automatic geolocation should remain an online convenience. Offline support does
not require removing it; offline support requires that geolocation failure does
not block boot, local web UI access, or manual latitude/longitude entry.

Plan:

- Keep the existing "Detect Location" behavior for online setups.
- If auto location fails, keep the device usable and surface the failure in the
  web UI.
- Make manual latitude and longitude entry available in the local web UI.
- Do not require the geolocation API for first-run setup.
- When preflashed firmware ships with a known default location, the setup flow
  requires verification or update before relying on zmanim.

Technical requirement:

IP geolocation remains online-only convenience behavior. Manual location entry
is the offline path.

### 5. ESPHome Build Files Fetch Packages From GitHub

The device YAML files currently use a GitHub remote package:

```yaml
packages:
  remote_package:
    url: https://github.com/chabad-source/melachaplug/
```

That is convenient for online ESPHome users, but it fails in offline build
environments. The repository already includes local package files under
`esphome/plugins/melachaplug/`, and the YAML files already show local include
examples in comments.

This is mainly a build-time issue. It does not affect devices that only use a
preflashed binary.

Plan:

- Keep the standard online example compatible with GitHub remote packages.
- Add offline build examples that use local includes:

```yaml
packages:
  melacha_config:  !include plugins/melachaplug/melacha_config.yaml
  plug_info:       !include plugins/melachaplug/plug_info.yaml
  location_info:   !include plugins/melachaplug/location_info.yaml
  optional_config: !include plugins/melachaplug/optional_config.yaml
```

- Publish release ZIP files that contain the exact YAML, headers, plugins, and
  binaries needed for offline flashing.
- Document that ESPHome itself and its platform dependencies must also be
  installed or cached before going offline.

Technical requirement:

Offline build examples and release bundles do not require GitHub package fetches.

### 6. Optional Packages May Fetch External Components

The optional `next_shabbbos_times` package contains:

```yaml
external_components:
  - source: github://pr#2933
    components: [ sun ]
  - source: github://pr#2955
    components: [ time ]
```

Using that package requires GitHub access at build time until those components
are vendored.

Plan:

- Treat this package as online-build-only.
- Keep it out of the offline-friendly preflashed profile.
- Vendor the required component code before adding this feature to any offline
  build bundle.

### 7. README Assets Are Hotlinked

The README uses remote images and badges, including GitHub-hosted screenshots,
badge services, donation button images, and other external image URLs.

This does not affect firmware behavior. It only affects offline documentation
viewing.

Plan:

- Use local image paths for screenshots that already exist in `images/`.
- Provide an offline-friendly release README in ZIP packages.
- Keep donation and status badges out of the offline-friendly release README.

Technical requirement:

README badges are documentation-only and are not firmware blockers.

## Technical Profiles

#### Online-Ready, Offline-Friendly Preflashed Profile

- Existing fail-safe boot behavior stays unchanged.
- SNTP uses internet NTP defaults.
- Auto location remains available.
- Web UI assets are embedded in firmware and external asset URLs are disabled.
- IP geolocation can run when internet is available, but failure does not block
  local setup.
- Manual latitude/longitude setup is documented as required.
- Timezone scope is explicit for the firmware image.
- Firmware exposes a clear status when time has not synced.
- Web UI allows entry of an explicit NTP server IP or hostname.
- Firmware tries the LAN gateway/router IP as the first NTP/SNTP candidate.

#### Offline Build Bundle

- Uses local ESPHome package includes.
- Includes headers, plugins, YAML files, and binaries.
- Does not depend on GitHub package fetches.
- Documents any ESPHome/platform dependencies that must be prepared before the
  build machine goes offline.

## Implementation Order

1. Documentation
   - Add this offline-environment note.
   - Update documentation to distinguish local logic from time/location
     dependencies.
   - Document that the preflashed firmware remains online-ready and fail-safe
     while adding offline-friendly local setup behavior.

2. Low-risk firmware/config changes
   - Embed ESPHome web UI assets in the shared web server config.
   - Make local package includes available for offline builds.

3. Offline-ready release flow
   - Publish an offline build ZIP per release.
   - Include precompiled binaries for supported plug models.
   - Include pinned web UI assets, local package files, and a short setup
     checklist for manual location and optional NTP IP configuration.

4. Better offline runtime support
   - Add configurable local NTP support.
   - Keep automatic geolocation as online convenience behavior, but make failure
     non-blocking for local setup.
   - Improve status text when time/location are missing or stale.

## TBD

TBD: What should the device do if it reboots and cannot get time: stay in
fail-safe Shabbos Mode, restore last known relay state, or show a setup/error
state?
