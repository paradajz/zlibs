# Filters

The `zlibs::utils::filters` module provides lightweight signal conditioning helpers. It is intended for inputs such as sensors, buttons and analog values.

## Public API

Public headers for this module are available in [`module/include/zlibs/utils/filters/`](../../../include/zlibs/utils/filters/).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_UTILS_FILTERS`: enables the filters module.

### Automatically selected symbols

This module does not automatically select additional Kconfig symbols.

## CMake library name

- `zlibs-utils-filters`

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_UTILS_FILTERS=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-utils-filters)
```

### Source code

#### EMA filter

```cpp
#include "zlibs/utils/filters/filters.h"

using namespace zlibs::utils::filters;

constexpr uint32_t EMA_SMOOTHING_PERCENTAGE = 50;

EmaFilter<uint16_t, EMA_SMOOTHING_PERCENTAGE> filter;
uint16_t smoothed = filter.value(100);
```

#### Median filter

```cpp
#include "zlibs/utils/filters/filters.h"

using namespace zlibs::utils::filters;

constexpr size_t MEDIAN_WINDOW_SIZE = 3;

MedianFilter<uint16_t, MEDIAN_WINDOW_SIZE> filter;
uint16_t first  = 10;
uint16_t second = 100;
uint16_t third  = 20;

filter.filter_value(first);
filter.filter_value(second);
auto median = filter.filter_value(third);    // median.value() == 20
```

#### Analog filter

```cpp
#include "zlibs/utils/filters/filters.h"

using namespace zlibs::utils::filters;

struct AnalogConfig
{
    static constexpr uint16_t MIN_RAW_READING_DIFF_SLOW            = 10;
    static constexpr uint16_t K_MIN_RAW_READING_DIFF_FAST          = 3;
    static constexpr uint32_t SLOW_FILTER_ENABLE_AFTER_INACTIVE_MS = 10;
    static constexpr uint16_t MEDIAN_FILTER_NUMBER_OF_READINGS     = 3;
    static constexpr uint16_t EMA_FILTER_SMOOTHING_PERCENTAGE      = 50;
    static constexpr bool     ENABLED                              = true;
};

constexpr size_t CHANNEL_COUNT = 2;

AnalogFilter<AnalogConfig, CHANNEL_COUNT> filter;
auto value = filter.filter_value(0, 512);
```

#### Debounce

```cpp
#include "zlibs/utils/filters/filters.h"

using namespace zlibs::utils::filters;

constexpr uint32_t DEBOUNCE_TIME_MS = 5;

Debounce debounce(DEBOUNCE_TIME_MS);
debounce.update(true);
bool current_state = debounce.state();
```
