# Settings

The `zlibs::utils::settings` module wraps the Zephyr settings subsystem with a typed settings structure. ZMS is automatically selected as a backend.

## Public API

Public headers for this module are available in [`module/include/zlibs/utils/settings/`](../../../include/zlibs/utils/settings/).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_UTILS_SETTINGS`: enables the settings module.

### Automatically selected symbols

- `CONFIG_FLASH`
- `CONFIG_FLASH_MAP`
- `CONFIG_SETTINGS`
- `CONFIG_ZMS`

## CMake library name

- `zlibs-utils-settings`

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_UTILS_SETTINGS=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-utils-settings)
```

### Source code

```cpp
#include "zlibs/utils/settings/settings.h"

using namespace zlibs::utils;

struct ChannelSetting
{
    struct Value
    {
        uint8_t channel;
    };

    struct Properties
    {
        static constexpr auto  KEY           = misc::StringLiteral{ "ChannelSetting" };
        static constexpr Value DEFAULT_VALUE = { .channel = 1 };
        static constexpr bool  KEEP_ON_RESET = true;
    };
};

using AppSettings = settings::SettingsManager<misc::StringLiteral{ "app" }, ChannelSetting>;

AppSettings manager;
manager.init();
manager.update<ChannelSetting>({ .channel = 3 });
```
