#!/usr/bin/env bash
# Build OpenDisplay nRF54 firmware with NCS/west.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="${APP_DIR:-${SCRIPT_DIR}/zephyr}"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build}"
BOARD="${BOARD:-xiao_nrf54l15/nrf54l15/cpuapp}"
PROFILE="${PROFILE:-battery}"   # battery | uart | quiet
PURGE="${PURGE:-always}"        # always | never | auto

export PROFILE

# shellcheck source=ncs-env.sh
source "${SCRIPT_DIR}/ncs-env.sh"

CMAKE_ARGS=(-DBOARD_ROOT="${SCRIPT_DIR}")
if [[ "${PROFILE}" == "uart" ]]; then
  CMAKE_ARGS+=(-DEXTRA_CONF_FILE="${APP_DIR}/prj_uart.conf")
fi
# PROFILE=quiet is picked up from the environment in CMakeLists.txt
# (prj_quiet.conf + OD_LOW_POWER_QUIET).
if [[ -n "${BUILD_VERSION:-}" ]]; then
  CMAKE_ARGS+=(-DBUILD_VERSION="${BUILD_VERSION}")
fi
if [[ -n "${SHA:-}" ]]; then
  CMAKE_ARGS+=(-DGIT_SHA="${SHA}")
fi
if [[ -n "${FACTORY_CONFIG_HEX:-}" ]]; then
  CMAKE_ARGS+=(-DFACTORY_CONFIG_HEX="${FACTORY_CONFIG_HEX}")
fi
if [[ "${OPENDISPLAY_FACTORY_CLEAR_CONFIG:-}" =~ ^(1|true|yes|on)$ ]]; then
  CMAKE_ARGS+=(-DFACTORY_CLEAR_CONFIG_ON_BOOT=ON)
fi

west build -p "${PURGE}" -d "${BUILD_DIR}" -b "${BOARD}" "${APP_DIR}" -- "${CMAKE_ARGS[@]}"

HEX="${BUILD_DIR}/merged.hex"
if [[ ! -f "${HEX}" ]]; then
  HEX="${BUILD_DIR}/zephyr/zephyr/zephyr.hex"
fi
if [[ ! -f "${HEX}" ]]; then
  HEX="${BUILD_DIR}/opendisplay_nrf54/zephyr/zephyr.hex"
fi

CONF="${BUILD_DIR}/zephyr/zephyr/.config"
echo
echo "Built: ${HEX}"
echo "Flash: ./flash.sh"
if [[ -f "${CONF}" ]]; then
  echo "Profile: ${PROFILE}  serial=$(grep -E '^CONFIG_SERIAL=|^# CONFIG_SERIAL is not set' "${CONF}" | head -1)"
  if [[ "${PROFILE}" == "uart" ]]; then
    echo "Serial monitor: 115200 baud on the board USB port"
  elif [[ "${PROFILE}" == "quiet" ]]; then
    echo "Quiet build: LOG off, no heartbeat prints (for advertising-current tests)"
  else
    echo "Logs: SEGGER RTT (battery build has no USB UART)"
  fi
fi
