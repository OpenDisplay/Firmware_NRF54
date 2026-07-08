cmake_minimum_required(VERSION 3.13.1)

set(BB_EPAPER_DIR ${CMAKE_CURRENT_LIST_DIR}/../src/bb_epaper)
set(UZLIB_DIR ${CMAKE_CURRENT_LIST_DIR}/../src/uzlib)
set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../src)

if(BOARD MATCHES ".*lm20.*")
  list(APPEND EXTRA_CONF_FILE ${CMAKE_CURRENT_LIST_DIR}/prj_lm20_extra.conf)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(opendisplay_nrf54)

target_include_directories(app PRIVATE
  ${SRC_DIR}
  ${BB_EPAPER_DIR}
  ${UZLIB_DIR}
)

target_compile_definitions(app PRIVATE TARGET_NRF54)

target_sources(app PRIVATE
  ${SRC_DIR}/main.c
  ${SRC_DIR}/board_nrf54.c
  ${SRC_DIR}/nrf54_gpio.c
  ${SRC_DIR}/nrf54_zephyr_time.c
  ${SRC_DIR}/newlib_stubs.c
  ${SRC_DIR}/opendisplay_ble.c
  ${SRC_DIR}/opendisplay_battery.c
  ${SRC_DIR}/opendisplay_i2c.c
  ${SRC_DIR}/opendisplay_sensor_sht40.c
  ${SRC_DIR}/opendisplay_sensor_bq27220.c
  ${SRC_DIR}/opendisplay_button.c
  ${SRC_DIR}/opendisplay_config_parser.c
  ${SRC_DIR}/opendisplay_config_storage.c
  ${SRC_DIR}/opendisplay_display_color.c
  ${SRC_DIR}/opendisplay_display.cpp
  ${SRC_DIR}/boot_screen.cpp
  ${SRC_DIR}/opendisplay_epd_map.c
  ${SRC_DIR}/opendisplay_led.c
  ${SRC_DIR}/opendisplay_touch.c
  ${SRC_DIR}/opendisplay_buzzer.c
  ${SRC_DIR}/opendisplay_pipe.c
  ${SRC_DIR}/qr/qrcode.c
  ${BB_EPAPER_DIR}/bb_epaper.cpp
  ${BB_EPAPER_DIR}/Group5.cpp
  ${UZLIB_DIR}/od_zlib_stream.c)
