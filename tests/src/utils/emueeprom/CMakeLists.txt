cmake_minimum_required(VERSION 3.20)
list(APPEND CONF_FILE "${CMAKE_CURRENT_LIST_DIR}/prj.conf")
include($ENV{ZEPHYR_WS}/zenv/cmake/test.cmake)

project(test)

include(CheckCXXSourceCompiles)

# Verify the compile-time API contract.

set(CMAKE_REQUIRED_INCLUDES "$ENV{ZENV_PROJECT_ROOT}/module/include")
set(CMAKE_REQUIRED_FLAGS "-std=gnu++20")

check_cxx_source_compiles("
    #include <span>
    #include <zlibs/utils/emueeprom/emueeprom.h>

    using namespace zlibs::utils::emueeprom;

    class MatchingHwa : public Hwa
    {
        public:
        bool init() override { return true; }
        bool erase_page(Page) override { return true; }
        bool write(Page, uint32_t, std::span<const uint8_t>) override { return true; }
        bool read(Page, uint32_t, std::span<uint8_t>) override { return true; }
    };

    int main()
    {
        MatchingHwa hwa;
        EmuEeprom<256, sizeof(uint32_t)> emu_eeprom(hwa);
        static_cast<void>(emu_eeprom.max_address());
        return 0;
    }
" EMUEEPROM_MATCHING_BACKEND_COMPILES)

if(NOT EMUEEPROM_MATCHING_BACKEND_COMPILES)
    message(FATAL_ERROR "emueeprom matching backend contract no longer compiles")
endif()

check_cxx_source_compiles("
    #include <span>
    #include <zlibs/utils/emueeprom/emueeprom.h>

    using namespace zlibs::utils::emueeprom;

    class MatchingHwa : public Hwa
    {
        public:
        bool init() override { return true; }
        bool erase_page(Page) override { return true; }
        bool write(Page, uint32_t, std::span<const uint8_t>) override { return true; }
        bool read(Page, uint32_t, std::span<uint8_t>) override { return true; }
    };

    int main()
    {
        MatchingHwa hwa;
        EmuEeprom<256, sizeof(uint32_t), address_count_for(256, sizeof(uint32_t))> emu_eeprom(hwa);
        static_cast<void>(emu_eeprom.max_address());
        return 0;
    }
" EMUEEPROM_EXPLICIT_MAX_ADDRESS_COUNT_COMPILES)

if(NOT EMUEEPROM_EXPLICIT_MAX_ADDRESS_COUNT_COMPILES)
    message(FATAL_ERROR "emueeprom explicit maximum address count no longer compiles")
endif()

check_cxx_source_compiles("
    #include <span>
    #include <zlibs/utils/emueeprom/emueeprom.h>

    using namespace zlibs::utils::emueeprom;

    class MatchingHwa : public Hwa
    {
        public:
        bool init() override { return true; }
        bool erase_page(Page) override { return true; }
        bool write(Page, uint32_t, std::span<const uint8_t>) override { return true; }
        bool read(Page, uint32_t, std::span<uint8_t>) override { return true; }
    };

    int main()
    {
        MatchingHwa hwa;
        [[maybe_unused]] EmuEeprom<256, sizeof(uint32_t), address_count_for(256, sizeof(uint32_t)) + 1> emu_eeprom(hwa);
        return 0;
    }
" EMUEEPROM_EXCESSIVE_ADDRESS_COUNT_COMPILES)

if(EMUEEPROM_EXCESSIVE_ADDRESS_COUNT_COMPILES)
    message(FATAL_ERROR "emueeprom excessive explicit address count unexpectedly compiles; constructor contract is broken")
endif()

check_cxx_source_compiles("
    #include <span>
    #include <zlibs/utils/emueeprom/emueeprom.h>

    using namespace zlibs::utils::emueeprom;

    class NotHwa
    {};

    int main()
    {
        NotHwa hwa;
        [[maybe_unused]] EmuEeprom<256, sizeof(uint32_t), 1> emu_eeprom(hwa);
        return 0;
    }
" EMUEEPROM_NON_HWA_BACKEND_COMPILES)

if(EMUEEPROM_NON_HWA_BACKEND_COMPILES)
    message(FATAL_ERROR "emueeprom non-HWA backend unexpectedly compiles; constructor contract is broken")
endif()

check_cxx_source_compiles("
    #include <span>
    #include <zlibs/utils/emueeprom/emueeprom.h>

    using namespace zlibs::utils::emueeprom;

    class InvalidPageSizeHwa : public Hwa
    {
        public:
        bool init() override { return true; }
        bool erase_page(Page) override { return true; }
        bool write(Page, uint32_t, std::span<const uint8_t>) override { return true; }
        bool read(Page, uint32_t, std::span<uint8_t>) override { return true; }
    };

    int main()
    {
        InvalidPageSizeHwa hwa;
        [[maybe_unused]] EmuEeprom<260, sizeof(uint32_t), 1> emu_eeprom(hwa);
        return 0;
    }
" EMUEEPROM_INVALID_PAGE_SIZE_COMPILES)

if(EMUEEPROM_INVALID_PAGE_SIZE_COMPILES)
    message(FATAL_ERROR "emueeprom invalid page size unexpectedly compiles; constructor contract is broken")
endif()

check_cxx_source_compiles("
    #include <span>
    #include <zlibs/utils/emueeprom/emueeprom.h>

    using namespace zlibs::utils::emueeprom;

    class NoLogicalSpaceHwa : public Hwa
    {
        public:
        bool init() override { return true; }
        bool erase_page(Page) override { return true; }
        bool write(Page, uint32_t, std::span<const uint8_t>) override { return true; }
        bool read(Page, uint32_t, std::span<uint8_t>) override { return true; }
    };

    int main()
    {
        NoLogicalSpaceHwa hwa;
        [[maybe_unused]] EmuEeprom<256, 256, 1> emu_eeprom(hwa);
        return 0;
    }
" EMUEEPROM_NO_LOGICAL_SPACE_BACKEND_COMPILES)

if(EMUEEPROM_NO_LOGICAL_SPACE_BACKEND_COMPILES)
    message(FATAL_ERROR "emueeprom backend without logical address space unexpectedly compiles; constructor contract is broken")
endif()

target_sources(app
    PRIVATE
    emueeprom_buffer_test.cpp
    emueeprom_zephyr_flash_test.cpp
)
