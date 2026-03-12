# USB Hardware Wrapper

The `zlibs` USB hardware wrapper provides a hardware-backed USB implementation built on top of Zephyr's new USB device stack.

This wrapper is implemented as a singleton because it manages a single USB device stack context.

## Public API

Public headers for this module are available in [`module/include/zlibs/drivers/usb/usb_hw.h`](../../../include/zlibs/drivers/usb/usb_hw.h).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_DRIVERS_USB_HW`: enables the hardware-backed USB wrapper.
- `CONFIG_ZLIBS_DRIVERS_USB_MANUFACTURER`: USB manufacturer string.
- `CONFIG_ZLIBS_DRIVERS_USB_PRODUCT`: USB product string.
- `CONFIG_ZLIBS_DRIVERS_USB_VID`: USB vendor ID.
- `CONFIG_ZLIBS_DRIVERS_USB_PID`: USB product ID.
- `CONFIG_ZLIBS_DRIVERS_USB_SELF_POWERED`: enables the self-powered USB attribute.
- `CONFIG_ZLIBS_DRIVERS_USB_REMOTE_WAKEUP`: enables the remote wakeup USB attribute.
- `CONFIG_ZLIBS_DRIVERS_USB_MAX_POWER`: USB `bMaxPower` value in 2 mA units.

### Automatically selected symbols

- `CONFIG_USB_DEVICE_STACK_NEXT`

## CMake library name

- `zlibs-drivers-usb_hw`

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_DRIVERS_USB_HW=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-drivers-usb_hw)
```

### Source code

```cpp
#include "zlibs/drivers/usb/usb_hw.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

using namespace zlibs::drivers::usb;

const device* DEVICE = DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0));

auto& usb = UsbHw::instance(DEVICE);

usb.init();
```
