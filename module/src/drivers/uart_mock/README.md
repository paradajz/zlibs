# UART Mock

The `zlibs` UART mock module provides a GoogleMock-based implementation of the zlibs UART interface. Intended for testing within the GTest framework.

## Public API

Public headers for this module are available in [`module/include/zlibs/drivers/uart/uart_mock.h`](../../../include/zlibs/drivers/uart/uart_mock.h).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_DRIVERS_UART_MOCK`: enables the mock UART implementation used by tests.

### Automatically selected symbols

This module does not automatically select additional Kconfig symbols.

## CMake library name

- `zlibs-drivers-uart_mock`

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_DRIVERS_UART_MOCK=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-drivers-uart_mock)
```

### Source code

```cpp
#include "zlibs/drivers/uart/uart_mock.h"

using namespace zlibs::drivers::uart;

UartMock uart;
uart.write(0x55);
```
