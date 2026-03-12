cmake_minimum_required(VERSION 3.20)
list(APPEND CONF_FILE "${CMAKE_CURRENT_LIST_DIR}/prj.conf")
include($ENV{ZEPHYR_WS}/zenv/cmake/test.cmake)

project(test)

target_sources(app
    PRIVATE
    midi_base_test.cpp
    midi_common_test.cpp
    midi_transport_ble_test.cpp
    midi_transport_serial_test.cpp
    midi_transport_usb_test.cpp
)
