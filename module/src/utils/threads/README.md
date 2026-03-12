# Threads

The `zlibs::utils::threads` module wraps Zephyr thread and workqueue primitives in reusable C++ helpers.

## Public API

Public headers for this module are available in [`module/include/zlibs/utils/threads/`](../../../include/zlibs/utils/threads/).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_UTILS_THREADS`: enables the threads module.

### Automatically selected symbols

- `CONFIG_THREADS`

## CMake library name

- `zlibs-utils-threads`

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_UTILS_THREADS=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-utils-threads)
```

### Source code

#### User thread

```cpp
#include "zlibs/utils/threads/threads.h"

using namespace zlibs::utils;

using UserThread =
    threads::UserThread<misc::StringLiteral{ "worker" }, K_PRIO_PREEMPT(0), 1024>;

UserThread thread([]() { // Work.
});
thread.run();
```

#### User thread with stop callback

```cpp
#include "zlibs/utils/threads/threads.h"

#include <atomic>

using namespace zlibs::utils;

using StoppableThread =
    threads::UserThread<misc::StringLiteral{ "stoppable-worker" }, K_PRIO_PREEMPT(0), 1024>;

std::atomic<bool> keep_running = true;

{
    StoppableThread thread(
        [&]()
        {
            while (keep_running.load(std::memory_order_acquire))
            {
                k_msleep(10);
            }
        },
        [&]()
        {
            keep_running.store(false, std::memory_order_release);
        });

    thread.run();
}
// thread destroyed -> exit callback runs -> loop stops
```

#### Workqueue thread

```cpp
#include "zlibs/utils/misc/kwork_delayable.h"
#include "zlibs/utils/threads/threads.h"

using namespace zlibs::utils;

using AppWorkqueue =
    threads::WorkqueueThread<misc::StringLiteral{ "app-workqueue" }, K_PRIO_PREEMPT(0), 1024>;

misc::KworkDelayable work(
    []()
    {
    },
    []()
    {
        return AppWorkqueue::handle();
    });
work.reschedule(0);
```
