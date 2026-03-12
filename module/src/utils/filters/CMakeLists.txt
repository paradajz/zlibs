zephyr_library_named(zlibs-utils-filters)

zephyr_library_sources(
    filters.cpp
)

target_include_directories(${ZEPHYR_CURRENT_LIBRARY}
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../include
)
