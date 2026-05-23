zephyr_library_named(zlibs_utils_sysex_conf)

zephyr_library_sources(
    sysex_conf.cpp
)

target_link_libraries(zlibs_utils_sysex_conf
    PUBLIC
    zlibs_utils_misc
)
