zephyr_library_named(zlibs-utils-midi)

zephyr_library_sources(
    transport/transport_ble.cpp
    transport/transport_serial.cpp
    transport/transport_usb.cpp
    midi.cpp
)
