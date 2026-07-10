#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# shellcheck source=ncs-env.sh
source "${SCRIPT_DIR}/ncs-env.sh"

west flash -d "${BUILD_DIR}" "$@"

HEX="${BUILD_DIR}/merged.hex"
if [[ ! -f "${HEX}" ]]; then
  HEX="${BUILD_DIR}/zephyr/zephyr/zephyr.hex"
fi

if command -v pyocd >/dev/null 2>&1 && [[ -f "${HEX}" ]]; then
  pyocd reset -t nrf54l 2>/dev/null || true
elif command -v nrfjprog >/dev/null 2>&1; then
  nrfjprog --reset 2>/dev/null || true
fi
