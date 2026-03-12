zephyr_library_named(zlibs-utils-motor_control)

zephyr_library_sources(
    motor_control.cpp
    motor.cpp
)

target_include_directories(${ZEPHYR_CURRENT_LIBRARY}
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../include
)
