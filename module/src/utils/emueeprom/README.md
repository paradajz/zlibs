# Emulated EEPROM

The `zlibs::utils::emueeprom` module provides a flash-backed EEPROM emulation layer based on STM32 application note AN3969. It stores 16-bit values in a pair of rotating flash pages and keeps a RAM mirror of the latest values for fast reads and simpler page transfers.

## Public API

Public headers for this module are available in [`module/include/zlibs/utils/emueeprom/`](../../../include/zlibs/utils/emueeprom/).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_UTILS_EMUEEPROM`: enables the emulated EEPROM module.

### Automatically selected symbols

This module does not automatically select additional Kconfig symbols.

## CMake library name

- `zlibs_utils_emueeprom`

## Notes

- `Entry` stores one logical `(address, value)` pair. The current entry type uses 16-bit addresses and 16-bit values.
- Entries are serialized into byte spans before they are written to the backend. The serialized format stores the value first, then the address.
- Logical address `0xFFFF` is reserved for internal page-status records and cannot be used for user data.
- Backends expose their native flash write granularity with `WRITE_BLOCK_SIZE`. When it is larger than one serialized entry, multiple entries are packed into one backend write block.
- `flush()` makes all pending state durable: RAM-only cached writes and any partially filled backend write block.
- There is no dedicated page header block. Page status records use the reserved status address and are appended through the same entry log; after `format()`, the first entries written are the runtime page status records.
- A few backend write blocks are kept out of the public logical address range so internal page-status records and page transfers have room to complete.
- The RAM mirror grows with the `EmuEeprom` logical address count, so expose only the address range users need.
- `format()` initializes runtime pages only by default.
- `format(true)` also erases the factory page.
- `restore_from_factory()` copies a valid factory page into runtime page 1.
- `store_to_factory()` snapshots the current valid runtime page into the factory page.

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_UTILS_EMUEEPROM=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs_utils_emueeprom)
```

### Source code

```cpp
#include "zlibs/utils/emueeprom/emueeprom.h"

#include <span>

using namespace zlibs::utils::emueeprom;

class MyHwa : public Hwa
{
    public:
    static constexpr size_t PAGE_SIZE = 2048;
    static constexpr size_t WRITE_BLOCK_SIZE = sizeof(uint32_t);

    bool init() override
    {
        return true;
    }

    bool erase_page([[maybe_unused]] Page page) override
    {
        return true;
    }

    bool write([[maybe_unused]] Page page,
               [[maybe_unused]] uint32_t offset,
               [[maybe_unused]] std::span<const uint8_t> data) override
    {
        return true;
    }

    bool read([[maybe_unused]] Page page,
              [[maybe_unused]] uint32_t offset,
              [[maybe_unused]] std::span<uint8_t> data) override
    {
        return true;
    }
};

MyHwa hwa;
EmuEeprom<MyHwa::PAGE_SIZE, MyHwa::WRITE_BLOCK_SIZE> eeprom(hwa);

eeprom.init();
eeprom.write(make_entry(0, 0x1234));

uint16_t value = 0;

if (eeprom.read(0, value) == ReadStatus::Ok)
{
    // Use the value.
}
```
