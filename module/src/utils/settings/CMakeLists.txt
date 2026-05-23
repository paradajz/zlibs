zephyr_library_named(zlibs_utils_settings)

zephyr_library_sources(
    settings.cpp
)

target_include_directories(${ZEPHYR_CURRENT_LIBRARY}
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/..
)
