# SysEx Configuration

The `zlibs::utils::sysex_conf` module implements a SysEx-based configuration protocol for MIDI devices. It supports GET, SET, BACKUP, and custom special requests over a user-defined block/section layout while delegating storage access and transport to a `DataHandler` implementation.

## Public API

Public headers for this module are available in [`module/include/zlibs/utils/sysex_conf/`](../../../include/zlibs/utils/sysex_conf/).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_UTILS_SYSEX_CONF`: enables the SysEx configuration protocol module.

### Automatically selected symbols

This module does not automatically select additional Kconfig symbols.

## CMake library name

- `zlibs-utils-sysex_conf`

## Protocol overview

- Each message uses a three-byte manufacturer ID configured through `ManufacturerId`.
- Configuration access is organized into blocks, sections, and parameter indexes.
- `GET` reads values from the target.
- `SET` writes values to the target.
- `BACKUP` reads values and reformats the response as a `SET` request payload.
- Built-in special requests open/close the connection and report protocol constants such as bytes-per-value and parameters-per-message.

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_UTILS_SYSEX_CONF=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-utils-sysex_conf)
```

### Source code

```cpp
#include "zlibs/utils/sysex_conf/sysex_conf.h"

using namespace zlibs::utils::sysex_conf;

class AppDataHandler : public DataHandler
{
    public:
    Status get(uint8_t block, uint8_t section, uint16_t index, uint16_t& value) override
    {
        value = 0;
        return Status::Ack;
    }

    Status set(uint8_t block, uint8_t section, uint16_t index, uint16_t new_value) override
    {
        return Status::Ack;
    }

    Status custom_request(uint16_t request, CustomResponse& custom_response) override
    {
        return Status::ErrorNotSupported;
    }

    void send_response(std::span<const uint8_t> response) override
    {
    }
};

AppDataHandler handler;
ManufacturerId manufacturer_id = { 0x00, 0x53, 0x43 };

static constexpr auto BLOCK = make_block(std::array<Section, 1>{
    Section{ 16, 0, 127 },
});

static constexpr auto LAYOUT = make_layout(std::array<Block, 1>{
    Block(BLOCK),
});

SysExConf protocol(handler, manufacturer_id);
protocol.set_layout(LAYOUT);
```
