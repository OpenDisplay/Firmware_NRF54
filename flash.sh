#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
BOARD="${BOARD:-}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

HEX="${BUILD_DIR}/merged.hex"
if [[ ! -f "${HEX}" ]]; then
  HEX="${BUILD_DIR}/zephyr/zephyr/zephyr.hex"
fi

pyocd_target() {
  if [[ "${BOARD}" == *lm20* || "${BUILD_DIR}" == *lm20* ]]; then
    echo nrf54lm20a
  else
    echo nrf54l
  fi
}

# Prefer system/user pyocd with a clean env. Sourcing ncs-env first breaks
# ~/.local pyocd (toolchain PYTHONPATH / SRE mismatch) and the NCS-bundled
# pyocd often lacks nrf54lm20a.
find_pyocd() {
  local candidate
  for candidate in \
    "${HOME}/.local/bin/pyocd" \
    /usr/local/bin/pyocd \
    "$(command -v pyocd 2>/dev/null || true)"; do
    if [[ -n "${candidate}" && -x "${candidate}" ]]; then
      echo "${candidate}"
      return 0
    fi
  done
  return 1
}

# west flash via Seeed CMSIS-DAP can leave the last bytes unprogrammed; that
# shows up as a BUS FAULT in net_buf during bt_enable (no advertising).
if PYOCD="$(find_pyocd)" && [[ -f "${HEX}" ]]; then
  TARGET="$(pyocd_target)"
  echo "Flashing ${HEX} via ${PYOCD} -t ${TARGET} (chip erase)"
  # Clear NCS python overrides if the user already sourced ncs-env in this shell.
  env -u PYTHONHOME -u PYTHONPATH -u PYTHONSTARTUP \
    "${PYOCD}" flash -t "${TARGET}" --erase chip "${HEX}"
  env -u PYTHONHOME -u PYTHONPATH -u PYTHONSTARTUP \
    "${PYOCD}" reset -t "${TARGET}" 2>/dev/null || true
  exit 0
fi

# Fallback: west flash (needs NCS toolchain on PATH).
# shellcheck source=ncs-env.sh
source "${SCRIPT_DIR}/ncs-env.sh"
west flash -d "${BUILD_DIR}" "$@"
if command -v nrfjprog >/dev/null 2>&1; then
  nrfjprog --reset 2>/dev/null || true
fi
