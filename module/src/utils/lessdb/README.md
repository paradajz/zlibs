# LessDb

The `zlibs::utils::lessdb` module provides a lightweight parameter database backed by a user-supplied storage medium. It handles all addressing, packing, and read-modify-write logic so callers work with typed block/section/parameter indices rather than raw addresses.

## Public API

Public headers for this module are available in [`module/include/zlibs/utils/lessdb/`](../../../include/zlibs/utils/lessdb/).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_UTILS_LESSDB`: enables the lessdb module.

### Automatically selected symbols

- `CONFIG_ZLIBS_UTILS_MISC`

## CMake library name

- `zlibs_utils_lessdb`

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_UTILS_LESSDB=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs_utils_lessdb)
```

### Source code

```cpp
#include "zlibs/utils/lessdb/lessdb.h"

using namespace zlibs::utils::lessdb;

// Define a custom HWA that reads and writes your storage medium.
class MyHwa : public Hwa {
    // ...
};

MyHwa hwa;
LessDb db(hwa);

// Describe the layout (compile-time)
static constexpr auto BLOCK = make_block(std::array<Section, 2>{
    Section{ 8, SectionParameterType::Bit, PreserveSetting::Disable, AutoIncrementSetting::Disable },
    Section{ 16, SectionParameterType::Byte, PreserveSetting::Enable, AutoIncrementSetting::Enable, 10 },
});

static constexpr auto LAYOUT = make_layout(std::array<Block, 1>{
    Block(BLOCK),
});

db.init();
db.set_layout(LAYOUT);
db.init_data(FactoryResetType::Full);

// Read and update parameters
uint32_t value = db.read(0, 1, 3).value_or(0);
db.update(0, 1, 3, 42);
```

Sections use a default value of zero when no explicit default is provided. `AutoIncrementSetting::Enable` increments the section default by parameter index during initialization.

Initialization defaults can also be supplied at runtime for a section:

```cpp
db.set_layout(LAYOUT);
db.register_layout_init_provider(0, 1, [](const InitRequest& request) -> std::optional<uint32_t> {
    if ((request.parameter_index % 2) == 0) {
        return 100 + request.parameter_index;
    }

    return std::nullopt;
});

db.init_data(FactoryResetType::Full);
```

Providers are scoped to the active layout UID, run during `init_data()`, and may return `std::nullopt` to keep the layout default. Use `clear_init_providers()` to remove registered providers.
