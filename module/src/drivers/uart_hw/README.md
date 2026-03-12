# UART Hardware Wrapper

The `zlibs` UART hardware wrapper provides an interrupt-driven UART implementation backed by a real Zephyr UART device.

## Public API

Public headers for this module are available in [`module/include/zlibs/drivers/uart/uart_hw.h`](../../../include/zlibs/drivers/uart/uart_hw.h).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_DRIVERS_UART_HW`: enables the hardware-backed UART wrapper.

### Automatically selected symbols

- `CONFIG_SERIAL`
- `CONFIG_RING_BUFFER`
- `CONFIG_UART_USE_RUNTIME_CONFIGURE`
- `CONFIG_UART_INTERRUPT_DRIVEN` when `CONFIG_ZLIBS_TESTING=n`

## CMake library name

- `zlibs-drivers-uart_hw`

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_DRIVERS_UART_HW=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-drivers-uart_hw)
```

### Source code

```cpp
#include "zlibs/drivers/uart/uart_hw.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

using namespace zlibs::drivers::uart;

RingBuffer<128> rx_buffer;
RingBuffer<128> tx_buffer;
const device* DEVICE = DEVICE_DT_GET(DT_NODELABEL(uart0));

UartHw uart(DEVICE);
Config cfg{115200, UART_CFG_STOP_BITS_1, rx_buffer, tx_buffer, false};

uart.init(cfg);
uart.write(0x55);
```
