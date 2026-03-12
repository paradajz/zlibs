cmake_minimum_required(VERSION 3.20)
list(APPEND CONF_FILE "${CMAKE_CURRENT_LIST_DIR}/prj.conf")
include($ENV{ZEPHYR_WS}/zenv/cmake/test.cmake)

project(test)

include(CheckCXXSourceCompiles)

# Verify compile-time API contract:
# runtime layout construction should fail to compile because sysex_conf uses consteval layout descriptors.

set(CMAKE_REQUIRED_INCLUDES "$ENV{ZEPHYR_PROJECT}/module/include")
set(CMAKE_REQUIRED_FLAGS "-std=gnu++20")

check_cxx_source_compiles("
    #include <zlibs/utils/sysex_conf/sysex_conf_common.h>

    using namespace zlibs::utils::sysex_conf;

    int main()
    {
        uint16_t runtime_parameter_count = 1;
        auto block_sections = std::array<Section, 1>{
            Section{ runtime_parameter_count, 0, 127 },
        };

        [[maybe_unused]] auto block = make_block(block_sections);
        return 0;
    }
" SYSEX_CONF_RUNTIME_LAYOUT_COMPILES)

if(SYSEX_CONF_RUNTIME_LAYOUT_COMPILES)
    message(FATAL_ERROR "sysex_conf runtime layout construction unexpectedly compiles; consteval contract is broken")
endif()

target_sources(app
    PRIVATE
    sysex_conf_test.cpp
)
