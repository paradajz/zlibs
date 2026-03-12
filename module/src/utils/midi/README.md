# MIDI

The `zlibs::utils::midi` module provides a transport-agnostic MIDI parser/transmitter adapted for this codebase. It supports serial MIDI, BLE MIDI, and USB MIDI through transport-specific hardware abstraction interfaces.

## Public API

Public headers for this module are available in [`module/include/zlibs/utils/midi/`](../../../include/zlibs/utils/midi/).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_UTILS_MIDI`: enables the MIDI module.
- `CONFIG_ZLIBS_UTILS_MIDI_SYSEX_ARRAY_SIZE`: maximum SysEx buffer size in bytes (default: `128`).
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

class MySerialHwa : public zlibs::utils::midi::serial::Hwa
{
    public:
    bool init() override;
    bool deinit() override;
    bool write(zlibs::utils::midi::serial::Packet& data) override;
    std::optional<zlibs::utils::midi::serial::Packet> read() override;
};

int main()
{
    MySerialHwa hwa;
    zlibs::utils::midi::serial::Serial midi(hwa);

    if (!midi.init())
    {
        return -1;
    }

    midi.send_note_on(60, 100, 1);
    midi.send_control_change(1, 64, 1);

    if (midi.read())
    {
        const auto& MESSAGE = midi.message();

        if (MESSAGE.type == zlibs::utils::midi::MessageType::NoteOn)
        {
            // Handle note-on event.
        }
    }

    (void)midi.deinit();
    return 0;
}
```

## License

This module is available under the MIT license. See the repository root [`LICENSE`](../../../../LICENSE).
