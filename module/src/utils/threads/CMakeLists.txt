zephyr_library_named(zlibs-utils-threads)

zephyr_library_sources(
    threads.cpp
)

target_include_directories(${ZEPHYR_CURRENT_LIBRARY}
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/..
)
