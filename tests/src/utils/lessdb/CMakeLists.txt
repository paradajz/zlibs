cmake_minimum_required(VERSION 3.20)
list(APPEND CONF_FILE "${CMAKE_CURRENT_LIST_DIR}/prj.conf")
include($ENV{ZEPHYR_WS}/zenv/cmake/test.cmake)

project(test)

include(CheckCXXSourceCompiles)

# Verify compile-time API contract:
# runtime layout construction should fail to compile because lessdb uses consteval factories.

set(CMAKE_REQUIRED_INCLUDES "$ENV{ZEPHYR_PROJECT}/module/include")
set(CMAKE_REQUIRED_FLAGS "-std=gnu++20")

check_cxx_source_compiles("
    #include <zlibs/utils/lessdb/lessdb_common.h>

    using namespace zlibs::utils::lessdb;

    int main()
    {
        size_t runtime_parameter_count = 1;
        auto block_sections = std::array<Section, 1>{
            Section{ runtime_parameter_count,
                     SectionParameterType::Byte,
                     PreserveSetting::Disable,
                     AutoIncrementSetting::Disable,
                     0u },
        };

        [[maybe_unused]] auto block = make_block(block_sections);
        return 0;
    }
" LESSDB_RUNTIME_LAYOUT_COMPILES)

if(LESSDB_RUNTIME_LAYOUT_COMPILES)
    message(FATAL_ERROR "lessdb runtime layout construction unexpectedly compiles; consteval contract is broken")
endif()

target_sources(app
    PRIVATE
    lessdb_test.cpp
)
