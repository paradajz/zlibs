# Misc Utilities

The `zlibs::utils::misc` module groups general-purpose support code used throughout the repository. It includes low-level helpers such as ring buffers, mutex wrappers, numeric and string utilities, date and time helpers, and supporting templates that are shared by higher-level modules.

## Public API

Public headers for this module are available in [`module/include/zlibs/utils/misc/`](../../../include/zlibs/utils/misc/).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_UTILS_MISC`: enables the miscellaneous utility module.

### Automatically selected symbols

This module does not automatically select additional Kconfig symbols.

## CMake library name

- `zlibs-utils-misc`

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_UTILS_MISC=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-utils-misc)
```

### Source code

#### Ring buffer

```cpp
#include "zlibs/utils/misc/ring_buffer.h"

using namespace zlibs::utils;

constexpr size_t BUFFER_CAPACITY = 8;

misc::RingBuffer<BUFFER_CAPACITY> buffer;
buffer.insert(0x2A);

auto next = buffer.remove();
```

#### String helpers

```cpp
#include "zlibs/utils/misc/string.h"

using namespace zlibs::utils;

constexpr auto ZLIBS_STRING = misc::StringLiteral{ "zlibs" };
constexpr auto SEP_STRING   = misc::StringLiteral{ "/" };
constexpr auto MISC_STRING  = misc::StringLiteral{ "misc" };
constexpr auto PATH_STRING  = misc::string_join(ZLIBS_STRING, SEP_STRING, MISC_STRING);
// PATH_STRING == "zlibs/misc"
```

#### Bit helpers

```cpp
#include "zlibs/utils/misc/bit.h"

using namespace zlibs::utils;

uint8_t value = 0;
misc::bit_set(value, 2);                      // value == 0b00000100
misc::bit_write(value, 5, true);              // value == 0b00100100
bool bit_5_is_set = misc::bit_read(value, 5); // bit_5_is_set == true
misc::bit_clear(value, 2);                    // value == 0b00100000
bool bit_2_is_set = misc::bit_read(value, 2); // bit_2_is_set == false
```

#### Numeric helpers

```cpp
#include "zlibs/utils/misc/numeric.h"

using namespace zlibs::utils;

int adc_raw     = 1200;
int adc_clamped = misc::constrain(adc_raw, 0, 1023);             // adc_clamped == 1023
int percent     = misc::map_range(adc_clamped, 0, 1023, 0, 100); // percent == 100
```

#### Date/time helpers

```cpp
#include "zlibs/utils/misc/datetime.h"

using namespace zlibs::utils;

misc::DateTime dt{
    .year = 2026, .month = 12, .mday = 31, .hours = 23, .minutes = 59, .seconds = 60,
};

misc::normalize_date_time(dt);
// dt is now 2027-01-01 00:00:00
```

#### Mutex wrapper

```cpp
#include "zlibs/utils/misc/mutex.h"

#include <atomic>

using namespace zlibs::utils;

misc::Mutex mutex;
std::atomic<int> shared_value = 0;

{
    const zlibs::utils::misc::LockGuard lock(mutex);
    shared_value.store(1, std::memory_order_release);
}

// mutex is released here
```
