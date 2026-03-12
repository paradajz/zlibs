# MIDI

The `zlibs::utils::midi` module provides a transport-agnostic MIDI parser/transmitter adapted for this codebase. It supports serial MIDI, BLE MIDI, and USB MIDI through transport-specific hardware abstraction interfaces.

## Public API

Public headers for this module are available in [`module/include/zlibs/utils/midi/`](../../../include/zlibs/utils/midi/).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_UTILS_MIDI`: enables the MIDI module.
- `CONFIG_ZLIBS_UTILS_MIDI_MAX_THRU_INTERFACES`: maximum number of registered MIDI thru interfaces.
- `CONFIG_ZLIBS_UTILS_MIDI_BLE_MAX_PACKET_SIZE`: maximum BLE MIDI packet size in bytes (default: `64`).

### Automatically selected symbols

This module does not automatically select additional Kconfig symbols.

## CMake library name

- `zlibs-utils-midi`

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_UTILS_MIDI=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-utils-midi)
```

### Source code

```cpp
#include "zlibs/utils/midi/transport/serial/transport_serial.h"

using namespace zlibs::utils::midi;

class MySerialHwa : public serial::Hwa
{
    public:
    bool init() override;
    bool deinit() override;
    bool write(uint8_t data) override;
    std::optional<uint8_t> read() override;
};

int main()
{
    constexpr uint8_t GROUP = 0;

    MySerialHwa hwa;
    serial::Serial midi(hwa);

    if (!midi.init())
    {
        return -1;
    }

    midi.send(midi1::note_on(GROUP, 0, 60, 100));
    midi.send(midi1::control_change(GROUP, 0, 1, 64));

    const auto PACKET = midi.read();

    if (PACKET.has_value() && (type(PACKET.value()) == MessageType::NoteOn))
    {
        // Handle note-on event.
    }

    (void)midi.deinit();
    return 0;
}
```

## License

This module is available under the MIT license. See the repository root [`LICENSE`](../../../../LICENSE).
