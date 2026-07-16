#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

OLD_IMAGE="${1:-$ROOT_DIR/bins/global-sonoff-s31.bin}"
BRIDGE_IMAGE="${2:-$ROOT_DIR/bins/global-sonoff-s31-ota-bridge.bin}"
FULL_IMAGE="${3:-$ROOT_DIR/bins/global-sonoff-s31-offline-friendly-ota.bin}"

# esp01_1m places the filesystem start at 0xfb000. ESP8266 OTA space is based
# on the installed sketch rounded to a flash sector, then ESPHome reserves one
# extra sector before handing size to Update.begin().
FS_START_BYTES="${FS_START_BYTES:-1028096}"
SECTOR_BYTES="${SECTOR_BYTES:-4096}"
MIN_HEADROOM_BYTES="${MIN_HEADROOM_BYTES:-16384}"

size_of() {
  local file="$1"

  if [[ ! -f "$file" ]]; then
    echo "Missing file: $file" >&2
    exit 2
  fi

  stat -c '%s' "$file"
}

round_up_sector() {
  local bytes="$1"

  echo $(( ((bytes + SECTOR_BYTES - 1) / SECTOR_BYTES) * SECTOR_BYTES ))
}

ota_max_after_installed_image() {
  local image_size="$1"
  local rounded_size
  local max_size

  rounded_size="$(round_up_sector "$image_size")"
  max_size=$(( ((FS_START_BYTES - rounded_size - SECTOR_BYTES) / SECTOR_BYTES) * SECTOR_BYTES ))

  if (( max_size < 0 )); then
    max_size=0
  fi

  echo "$max_size"
}

OLD_SIZE="$(size_of "$OLD_IMAGE")"
BRIDGE_SIZE="$(size_of "$BRIDGE_IMAGE")"
FULL_SIZE="$(size_of "$FULL_IMAGE")"

OLD_OTA_MAX="$(ota_max_after_installed_image "$OLD_SIZE")"
BRIDGE_OTA_MAX="$(ota_max_after_installed_image "$BRIDGE_SIZE")"
BRIDGE_HEADROOM=$((BRIDGE_OTA_MAX - FULL_SIZE))

echo "S31 OTA room check"
echo "old image:        $OLD_SIZE bytes"
echo "bridge image:     $BRIDGE_SIZE bytes"
echo "full image:       $FULL_SIZE bytes"
echo "old OTA max:      $OLD_OTA_MAX bytes"
echo "bridge OTA max:   $BRIDGE_OTA_MAX bytes"
echo "bridge headroom:  $BRIDGE_HEADROOM bytes"
echo "required reserve: $MIN_HEADROOM_BYTES bytes"

failed=0

if (( BRIDGE_SIZE > OLD_OTA_MAX )); then
  echo "FAIL: old S31 image cannot OTA-flash the bridge." >&2
  failed=1
fi

if (( FULL_SIZE + MIN_HEADROOM_BYTES > BRIDGE_OTA_MAX )); then
  echo "FAIL: bridge does not leave enough OTA room for the full image plus reserve." >&2
  failed=1
fi

if (( FULL_SIZE > OLD_OTA_MAX )); then
  echo "expected: old S31 image cannot OTA-flash the full offline-friendly image directly"
fi

if (( failed != 0 )); then
  exit 1
fi

echo "PASS: S31 two-step OTA path has enough room."
