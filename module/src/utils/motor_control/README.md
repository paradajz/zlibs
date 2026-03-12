# Motor Control

The `zlibs::utils::motor_control` module provides abstractions for coordinating one or more motors and running scheduled movement updates.

## Public API

Public headers for this module are available in [`module/include/zlibs/utils/motor_control/`](../../../include/zlibs/utils/motor_control/).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_UTILS_MOTOR_CONTROL`: enables the motor control module.

### Automatically selected symbols

This module does not automatically select additional Kconfig symbols.

## CMake library name

- `zlibs-utils-motor_control`

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_UTILS_MOTOR_CONTROL=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-utils-motor_control)
```

### Source code

```cpp
#include "zlibs/utils/motor_control/motor_control.h"

#include <array>

using namespace zlibs::utils::motor_control;

class ExampleHwa : public Hwa
{
    public:
    bool init() override { return true; }
    bool enable() override { return true; }
    bool disable() override { return true; }
    bool write_step(bool) override { return true; }
    bool toggle_step() override { return true; }
    bool set_direction(Direction) override { return true; }
};

ExampleHwa                                   hwa;
Config                                       cfg;
Motor                                        motor(hwa, cfg);
std::array<std::reference_wrapper<Motor>, 1> motors = { motor };
MotorControl                                 control(motors);

control.init();

Movement movement{
    .direction = Direction::Cw,
    .speed     = static_cast<uint32_t>(Config::DEFAULT_END_INTERVAL),
    .pulses    = 100,
};

control.setup_movement(0, movement);
```
