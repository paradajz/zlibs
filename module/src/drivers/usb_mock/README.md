# USB Mock

The `zlibs` USB mock module provides a GoogleMock-based implementation of the zlibs USB interface. Intended for testing within the GTest framework.

## Public API

Public headers for this module are available in [`module/include/zlibs/drivers/usb/usb_mock.h`](../../../include/zlibs/drivers/usb/usb_mock.h).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_DRIVERS_USB_MOCK`: enables the mock USB implementation used by tests.

### Automatically selected symbols

This module does not automatically select additional Kconfig symbols.

## CMake library name

- `zlibs-drivers-usb_mock`

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_DRIVERS_USB_MOCK=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-drivers-usb_mock)
```

### Source code

```cpp
#include "zlibs/drivers/usb/usb_mock.h"

using namespace zlibs::drivers::usb;

UsbMock usb;
usb.init();
```
