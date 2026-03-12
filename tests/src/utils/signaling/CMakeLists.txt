cmake_minimum_required(VERSION 3.20)
list(APPEND CONF_FILE "${CMAKE_CURRENT_LIST_DIR}/prj.conf")
include($ENV{ZEPHYR_WS}/zenv/cmake/test.cmake)

project(test)

target_sources(app
    PRIVATE
    signaling_test.cpp
)

add_dependencies(app
    zlibs-utils-signaling
)

target_link_libraries(app
    PUBLIC
    zlibs-utils-signaling
)
