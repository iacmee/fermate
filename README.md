# Fermate

A Linux utility that temporarily grabs evdev devices in `/dev/input`, including
those connected while it is running. Devices are released when the timer expires
or when you enter the random unlock string, if `--challenge` is enabled.
This is not an authenticated screen lock.

“Fermate” is Romano (the dialect of Rome) for “stop” or “take a break.”

## Why this project exists

I built Fermate because I sometimes spend too much time at the computer,
programming without taking breaks. It forces me to stop for a while by blocking
the keyboard and mouse until the timer expires, so I cannot just keep coding
instead of taking a break.

For emergencies, `--challenge` provides a random string that can be typed to
unlock early. It adds a deliberate step before returning to the computer.
The simple `exit` command is reserved for the test build so it does not become
an easy shortcut during normal use.

Fermate is a personal tool for taking breaks, not a security boundary: it does
not prevent someone determined to stop the process or restart the machine.

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

`--challenge` displays a random 32-character string of ASCII letters and digits.
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

## Keep the screen awake on GNOME

Because Fermate grabs input events, the desktop does not receive your keyboard
and mouse activity. It may therefore dim or turn off the screen, or lock the
session, according to its inactivity settings, even while you are typing into
Fermate.

To prevent idle behavior during a run on GNOME, launch it from your desktop
terminal with:

```sh
gnome-session-inhibit --inhibit idle sudo ./main 300 --challenge
```

For the test build:

```sh
gnome-session-inhibit --inhibit idle sudo ./main-test 30 --challenge
```

Keep `gnome-session-inhibit` outside `sudo` so it runs as your desktop user.
It requests idle inhibition while Fermate is running and releases it when the
command exits, without changing desktop settings. Fermate itself does not
manage screen power or create an inhibitor; running `sudo ./main` directly
keeps the desktop's normal inactivity behavior.

This optional workaround requires `gnome-session-inhibit` (provided by
`gnome-session-bin` on Ubuntu). See the [GNOME documentation](https://gnome.pages.gitlab.gnome.org/gnome-session/re02.html).
