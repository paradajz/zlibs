zephyr_library_named(zlibs-utils-midi)

zephyr_library_sources(
    transport/ble.cpp
    transport/serial.cpp
    transport/usb.cpp
    midi.cpp
)
