# OpenDisplay — nRF54 (XIAO nRF54L15 / nRF54LM20A)

Bluetooth LE ePaper firmware built with **PlatformIO + Zephyr** and **bb_epaper**.

## Prerequisites

```bash
pio pkg install -g -p "https://github.com/Seeed-Studio/platform-seeedboards.git"
```

## Build

```bash
cd Firmware-nrf54
pio run -e seeed-xiao-nrf54l15
pio run -e seeed-xiao-nrf54lm20a
```

## Flash

```bash
pio run -e seeed-xiao-nrf54l15 -t upload
```

## Pin encoding

OpenDisplay configs use compact `(port << 4) | pin` bytes (e.g. `P2.02` → `0x22`). Use the nRF54 toolbox presets — do not reuse nRF52840 GPIO numbers.

## Layout

- `src/` — OpenDisplay application (BLE, protocol, display via bb_epaper)
- `third_party/bb_epaper/` — vendored library with `nrf54_zephyr_io.inl`
- `zephyr/CMakeLists.txt` — Zephyr build integration
- `src/bb_epaper`, `src/uzlib` — symlinks to `third_party/` (recreated by `scripts/factory_config_gen.py` on each build)
