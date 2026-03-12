# zlibs

`zlibs` is a collection of various Zephyr drivers, structures and subsystems as well as general purpose libraries and helpers meant to be used within Zephyr RTOS. The repository can be used in Zephyr projects simply by adding it to `west.yml`. This repository is based on [`ztemplate`](https://github.com/paradajz/ztemplate) repository - for more information about the setup check the [`README`](https://github.com/paradajz/ztemplate/blob/master/README.md) there.

## Index

### Drivers

* UART
  - [UART Hardware Wrapper](module/src/drivers/uart_hw/README.md): UART implementation using interrupt-driven Zephyr UART API.
  - [UART Mock](module/src/drivers/uart_mock/README.md): GoogleMock-based UART implementation for tests.
* USB
  - [USB Hardware Wrapper](module/src/drivers/usb_hw/README.md): USB implementation built on top of Zephyr's USB device stack.
  - [USB Mock](module/src/drivers/usb_mock/README.md): GoogleMock-based USB implementation for tests.

### Utilities

- [Emulated EEPROM](module/src/utils/emueeprom/README.md): flash-backed EEPROM emulation with rotating pages and a RAM mirror.
- [Filters](module/src/utils/filters/README.md): signal conditioning helpers such as EMA, median, analog filtering, and debounce logic.
- [LessDb](module/src/utils/lessdb/README.md): lightweight parameter database backed by a user-supplied storage medium.
- [Misc Utilities](module/src/utils/misc/README.md): general-purpose helpers including ring buffers, mutex wrappers, numeric utilities, string helpers and more.
- [MIDI](module/src/utils/midi/README.md): transport-agnostic MIDI parser/transmitter with serial, BLE, and USB backends.
- [Motor Control](module/src/utils/motor_control/README.md): stepper motor control.
- [Settings](module/src/utils/settings/README.md): wrapper around the Zephyr settings subsystem.
- [Signaling](module/src/utils/signaling/README.md): asynchronous publish/subscribe infrastructure with queued dispatch.
- [State Machine](module/src/utils/state_machine/README.md): compile-time checked finite state machine.
- [SysEx Configuration](module/src/utils/sysex_conf/README.md): SysEx-based configuration protocol with GET/SET/BACKUP and custom requests.
- [Threads](module/src/utils/threads/README.md): wrappers around Zephyr threads and workqueues.

## Usage

Add the following to your `west.yml`:

``` yaml
manifest:
  remotes:
  - name: paradajz
    url-base: https://github.com/paradajz

  projects:
    - name: zlibs
      remote: paradajz
      revision: master
      path: modules/zlibs
```

For the usage of individual modules check their respective READMEs.
