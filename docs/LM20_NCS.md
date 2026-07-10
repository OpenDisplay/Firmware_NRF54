# Seeed XIAO nRF54LM20A on NCS

NCS v3.3.1 does not include a `seeed-xiao-nrf54lm20a` board definition. OpenDisplay LM20 support is **not built in CI** until a board port lands.

## Bring-up path

1. Copy Seeed LM20 DTS from [platform-seeedboards](https://github.com/Seeed-Studio/platform-seeedboards) into `zephyr/boards/seeed/` (out-of-tree) or upstream to Zephyr.
2. Build with:
   ```bash
   BOARD=seeed-xiao-nrf54lm20a/nrf54lm20a/cpuapp ./build.sh
   ```
3. Retain `prj_lm20_extra.conf` (disables `CONFIG_BT_CTLR_ASSERT_OPTIMIZE_FOR_SIZE`).

Until then, use **XIAO nRF54L15** (`./build.sh` default) for production builds.
