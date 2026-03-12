cmake_minimum_required(VERSION 3.20)
list(APPEND CONF_FILE "${CMAKE_CURRENT_LIST_DIR}/prj.conf")
include($ENV{ZEPHYR_WS}/zenv/cmake/test.cmake)

project(test)

target_sources(app
    PRIVATE
    bit_test.cpp
    datetime_test.cpp
    meta_test.cpp
    mutex_test.cpp
    noncopyable_test.cpp
    numeric_test.cpp
    ring_buffer_test.cpp
    string_test.cpp
    timer_test.cpp
    version_test.cpp
)
