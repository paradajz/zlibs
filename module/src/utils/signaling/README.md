# Signaling

The `zlibs::utils::signaling` module provides asynchronous publish/subscribe delivery with typed subscriptions and (optional) derived observables.

## Public API

Public headers for this module are available in [`module/include/zlibs/utils/signaling/`](../../../include/zlibs/utils/signaling/).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_UTILS_SIGNALING`: enables the signaling module.
- `CONFIG_ZLIBS_UTILS_SIGNALING_MAX_PAYLOAD_LENGTH_BYTES`: sets the maximum size of one queued signal payload.
- `CONFIG_ZLIBS_UTILS_SIGNALING_MAX_POOL_SIZE`: sets the number of preallocated queued dispatch nodes.
- `CONFIG_ZLIBS_UTILS_SIGNALING_THREAD_PRIORITY`: sets the priority of the dedicated signaling dispatcher thread.
- `CONFIG_ZLIBS_UTILS_SIGNALING_THREAD_STACK_SIZE`: sets the stack size of the dedicated signaling dispatcher thread.

### Automatically selected symbols

- `CONFIG_ZLIBS_UTILS_THREADS`

## CMake library name

- `zlibs-utils-signaling`

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_UTILS_SIGNALING=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-utils-signaling)
```

### Source code

```cpp
#include "zlibs/utils/signaling/signaling.h"

using namespace zlibs::utils::signaling;

struct Data
{
    uint32_t value;
};

auto sub_direct = subscribe<Data>([]([[maybe_unused]] const Data& data)
                                  {
                                  });

auto sub_observable = observe<Data>()
                          .only_if([](const Data& data)
                                   {
                                       return data.value > 10;
                                   })
                          .subscribe([]([[maybe_unused]] const Data& data)
                                     {
                                     });

publish(Data{ .value = 42 });
```
