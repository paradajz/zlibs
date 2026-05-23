target_sources(app
    PRIVATE
    src/main.cpp
)

target_link_libraries(app
    PUBLIC
    zlibs_drivers_uart_hw
    zlibs_utils_filters
    zlibs_utils_misc
    zlibs_utils_motor_control
    zlibs_utils_settings
    zlibs_utils_signaling
    zlibs_utils_state_machine
    zlibs_utils_threads
)
