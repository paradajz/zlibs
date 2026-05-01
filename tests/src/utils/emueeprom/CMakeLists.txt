cmake_minimum_required(VERSION 3.20)
list(APPEND CONF_FILE "${CMAKE_CURRENT_LIST_DIR}/prj.conf")
include($ENV{ZEPHYR_WS}/zenv/cmake/test.cmake)

project(test)

include(CheckCXXSourceCompiles)

# Verify compile-time API contract:
# matching backend page sizes should compile, while missing or mismatched
# page sizes should fail because EmuEeprom now requires compile-time HWA
# capacities.

set(CMAKE_REQUIRED_INCLUDES "$ENV{ZEPHYR_PROJECT}/module/include")
set(CMAKE_REQUIRED_FLAGS "-std=gnu++20")

check_cxx_source_compiles("
    #define CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE 128
    #include <span>
    #include <zlibs/utils/emueeprom/emueeprom.h>

    using namespace zlibs::utils::emueeprom;

    class MatchingHwa : public Hwa
    {
        public:
        static constexpr size_t WRITE_BLOCK_SIZE = sizeof(uint32_t);
        static constexpr size_t PAGE_1_SIZE = CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE;
        static constexpr size_t PAGE_2_SIZE = CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE;
        static constexpr size_t FACTORY_SIZE = CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE;

        bool init() override { return true; }
        bool erase_page(Page) override { return true; }
        bool write(Page, uint32_t, std::span<const uint8_t>) override { return true; }
        bool read(Page, uint32_t, std::span<uint8_t>) override { return true; }
    };

    int main()
    {
        MatchingHwa hwa;
        EmuEeprom emu_eeprom(hwa);
        static_cast<void>(emu_eeprom.max_address());
        return 0;
    }
" EMUEEPROM_MATCHING_BACKEND_COMPILES)

if(NOT EMUEEPROM_MATCHING_BACKEND_COMPILES)
    message(FATAL_ERROR "emueeprom matching backend contract no longer compiles")
endif()

check_cxx_source_compiles("
    #define CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE 128
    #include <span>
    #include <zlibs/utils/emueeprom/emueeprom.h>

    using namespace zlibs::utils::emueeprom;

    class MissingSizesHwa : public Hwa
    {
        public:
        bool init() override { return true; }
        bool erase_page(Page) override { return true; }
        bool write(Page, uint32_t, std::span<const uint8_t>) override { return true; }
        bool read(Page, uint32_t, std::span<uint8_t>) override { return true; }
    };

    int main()
    {
        MissingSizesHwa hwa;
        [[maybe_unused]] EmuEeprom emu_eeprom(hwa);
        return 0;
    }
" EMUEEPROM_MISSING_BACKEND_SIZES_COMPILES)

if(EMUEEPROM_MISSING_BACKEND_SIZES_COMPILES)
    message(FATAL_ERROR "emueeprom backend without page-size constants unexpectedly compiles; constructor contract is broken")
endif()

check_cxx_source_compiles("
    #define CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE 128
    #include <span>
    #include <zlibs/utils/emueeprom/emueeprom.h>

    using namespace zlibs::utils::emueeprom;

    class MismatchedHwa : public Hwa
    {
        public:
        static constexpr size_t WRITE_BLOCK_SIZE = sizeof(uint32_t);
        static constexpr size_t PAGE_1_SIZE = CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE;
        static constexpr size_t PAGE_2_SIZE = CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE + 4;
        static constexpr size_t FACTORY_SIZE = CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE;

        bool init() override { return true; }
        bool erase_page(Page) override { return true; }
        bool write(Page, uint32_t, std::span<const uint8_t>) override { return true; }
        bool read(Page, uint32_t, std::span<uint8_t>) override { return true; }
    };

    int main()
    {
        MismatchedHwa hwa;
        [[maybe_unused]] EmuEeprom emu_eeprom(hwa);
        return 0;
    }
" EMUEEPROM_MISMATCHED_BACKEND_COMPILES)

if(EMUEEPROM_MISMATCHED_BACKEND_COMPILES)
    message(FATAL_ERROR "emueeprom mismatched backend sizes unexpectedly compile; constructor contract is broken")
endif()

target_sources(app
    PRIVATE
    emueeprom_buffer_test.cpp
    emueeprom_zephyr_flash_test.cpp
)
