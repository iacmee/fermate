# Fermate

A Linux utility that temporarily grabs evdev devices in `/dev/input`, including
those connected while it is running. Devices are released when the timer expires
or when you enter the random unlock string, if `--challenge` is enabled.
This is not an authenticated screen lock.

## Build and run

Requires a C compiler, Make, Linux headers, libxkbcommon development headers and
libraries, and XKB keyboard data. On Debian/Ubuntu, install
`build-essential libxkbcommon-dev xkb-data`.

```sh
make
./main --help
sudo ./main 300
sudo ./main 300 --challenge
```

The duration is a positive number of seconds, up to `2147483647`; the default is
300. Invalid arguments are rejected before any devices are grabbed. Building
does not require privileges. Running requires access to input device nodes and
permission to grab them exclusively, usually through root.

`--challenge` displays a random 12-character string of ASCII letters and digits.
Type it and press Enter to unlock early. Without this option, wait for the timer.
The `exit` command is available only in the test build.

The random generator uses `getrandom` and supports ASCII letters, digits, and
symbols. UTF-8 character classes are not implemented and are explicitly rejected.

The default keyboard layout is Italian. To change it:

```sh
sudo env XKB_DEFAULT_LAYOUT=us ./main 60
```

## Commands and input

Typed text is displayed in the terminal. Type `status` and press Enter to see the
remaining seconds, rounded up. An incorrect entry displays an error and clears
the line. Backspace removes one UTF-8 character. Lines exceeding 1023 bytes are
cleared. Each keyboard maintains its own modifier and compose state; text from
all keyboards goes into the same input line.

Messages use colors in a terminal. Set `NO_COLOR` to disable colors.

## Test build

```sh
make testing
sudo ./main-test 10
```

This build also allows early unlocking with `exit` followed by Enter.
`make` builds `./main` without that command. The two executables use separate
object files and can coexist. `make testing` builds the manual test version;
it does not run automated tests.

## Device release and limitations

The timer uses a monotonic clock and works even without keyboards. SIGINT,
SIGTERM, and SIGHUP stop the program and release the devices. Grab or read errors
also release the devices and cause a failure exit. If the process is forcibly
terminated, the kernel closes its descriptors and releases its grabs.

Real hardware verification is still needed for mouse and keyboard blocking,
multiple keyboards, removal and reconnection, modifiers, compose sequences, and
layouts. Hotplug detection follows the kernel notification, leaving a short
interval between connection and grabbing. Time spent suspended does not count
towards the timer.
