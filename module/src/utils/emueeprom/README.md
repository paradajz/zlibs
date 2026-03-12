# Emulated EEPROM

The `zlibs::utils::emueeprom` module provides a flash-backed EEPROM emulation layer based on STM32 application note AN3969. It stores 16-bit values in a pair of rotating flash pages and keeps a RAM mirror of the latest values for fast reads and simpler page transfers.

## Public API

Public headers for this module are available in [`module/include/zlibs/utils/emueeprom/`](../../../include/zlibs/utils/emueeprom/).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_UTILS_EMUEEPROM`: enables the emulated EEPROM module.
- `CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE`: sets the flash page size, in bytes, exposed through the `Hwa` interface.

### Automatically selected symbols

This module does not automatically select additional Kconfig symbols.

## CMake library name

- `zlibs-utils-emueeprom`

## Notes

- One 32-bit flash word stores one `(address, value)` pair, so the usable logical address count per page is `(page_size / 4) - 1`.
- The RAM mirror grows with the configured page size, so larger flash pages also increase RAM usage.
- When `use_factory_page` is enabled, `format()` copies a valid factory page into runtime page 1.

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_UTILS_EMUEEPROM=y
CONFIG_ZLIBS_UTILS_EMUEEPROM_PAGE_SIZE=2048
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-utils-emueeprom)
```

### Source code

```cpp
#include "zlibs/utils/emueeprom/emueeprom.h"

using namespace zlibs::utils::emueeprom;

class MyHwa : public Hwa
{
    public:
    bool init() override
    {
        return true;
    }

    bool erase_page([[maybe_unused]] Page page) override
    {
        return true;
    }

    bool write_32([[maybe_unused]] Page page,
                  [[maybe_unused]] uint32_t address,
                  [[maybe_unused]] uint32_t data) override
    {
        return true;
    }

    std::optional<uint32_t> read_32([[maybe_unused]] Page page,
                                    [[maybe_unused]] uint32_t address) override
    {
        return 0xFFFFFFFFu;
    }
};

MyHwa hwa;
EmuEeprom eeprom(hwa, true);

eeprom.init();
eeprom.write(0, 0x1234);

uint16_t value = 0;

if (eeprom.read(0, value) == ReadStatus::Ok)
{
    // use value
}
```
