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

- `zlibs-utils-lessdb`

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_UTILS_LESSDB=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-utils-lessdb)
```

### Source code

```cpp
#include "zlibs/utils/lessdb/lessdb.h"

using namespace zlibs::utils::lessdb;

// Define a custom HWA that reads/writes your storage medium
class MyHwa : public Hwa { /* ... */ };

MyHwa hwa;
LessDb db(hwa);

// Describe the layout (compile-time)
static constexpr auto BLOCK = make_block(std::array<Section, 2>{
    Section{ 8, SectionParameterType::Bit, PreserveSetting::Disable, AutoIncrementSetting::Disable, 0 },
    Section{ 16, SectionParameterType::Byte, PreserveSetting::Enable, AutoIncrementSetting::Enable, 10 },
});

static constexpr auto LAYOUT = make_layout(std::array<Block, 1>{
    Block{ BLOCK },
});

db.init();
db.set_layout(LAYOUT);
db.init_data(FactoryResetType::Full);

// Read and update parameters
uint32_t value = db.read(0, 1, 3).value_or(0);
db.update(0, 1, 3, 42);
```
