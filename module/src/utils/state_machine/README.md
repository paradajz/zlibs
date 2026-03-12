# State Machine

The `zlibs::utils::state_machine` module provides a compile-time checked finite state machine framework.

## Public API

Public headers for this module are available in [`module/include/zlibs/utils/state_machine/`](../../../include/zlibs/utils/state_machine/).

## Kconfig symbols

### Configuration symbols

- `CONFIG_ZLIBS_UTILS_STATE_MACHINE`: enables the state machine module.

### Automatically selected symbols

This module does not automatically select additional Kconfig symbols.

## CMake library name

- `zlibs-utils-state_machine`

## Example usage

### `prj.conf`

```conf
CONFIG_ZLIBS_UTILS_STATE_MACHINE=y
```

### `CMakeLists.txt`

```cmake
target_link_libraries(app PRIVATE zlibs-utils-state_machine)
```

### Source code

```cpp
#include "zlibs/utils/state_machine/state_machine.h"

using namespace zlibs::utils::state_machine;

struct Event
{
    bool start = false;
};

struct IdleState;
struct RunState;

struct InitState : InitStateTag
{
    bool enter() { return true; }
    bool exit() { return true; }

    Transition<IdleState> handle(const Event&)
    {
        return Transition<IdleState>{};
    }
};

struct ErrorState : ErrorStateTag
{
    bool enter() { return true; }
    bool exit() { return true; }
};

struct IdleState
{
    bool enter() { return true; }
    bool exit() { return true; }

    Maybe<Transition<RunState>> handle(const Event& event)
    {
        if (event.start)
        {
            return Transition<RunState>{};
        }

        return Nothing{};
    }
};

struct RunState
{
    bool enter() { return true; }
    bool exit() { return true; }
};

StateMachine<InitState, ErrorState, IdleState, RunState> sm;
sm.init();
sm.handle(Event{});                 // transitions InitState -> IdleState
sm.handle(Event{ .start = true });  // transitions IdleState -> RunState
```
