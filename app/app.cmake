target_sources(app
    PRIVATE
    src/main.cpp
)

target_link_libraries(app
    PUBLIC
    zlibs-drivers-uart_hw
    zlibs-utils-filters
    zlibs-utils-misc
    zlibs-utils-motor_control
    zlibs-utils-settings
    zlibs-utils-signaling
    zlibs-utils-state_machine
    zlibs-utils-threads
)
