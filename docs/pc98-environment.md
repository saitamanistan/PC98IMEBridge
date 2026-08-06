# PC-98 Development Environment

## Verified emulator setup

The repository's primary PC-98 environment is np21w rev103:

```text
Executable: D:\np21w\np21w-starterset\np21x64w.exe
Boot image: D:\np21w\np21w-starterset\fdosboot.hdi
Host share: D:\np21w\np21w-starterset\share  (guest Z:)
```

Configure Serial/Parallel COM1 as follows:

```text
Type:        PIPE
Pipe Name:   NP2-NamedPipe
Server Name: .
Speed:       19200 bps
```

Do not select np21w COM3; it is a different device. The verified normal COM1
I/O addresses are data `0030h` and status/control `0032h`. `0130h/0132h` are
np21w FIFO-mode registers.

See [np21w COM and Named Pipe setup](np21w-serial-pipe.md) for the exact GUI
steps, the distinction between guest COM1 and Windows physical COM names, and
the synchronized procedure for changing the main or diagnostic pipe name.

## Build and deploy

```sh
make pc98-tsr
make pc98 pc98-sys pc98-device-test
```

Copy `build/pc98/IME98TSR.COM`, `samples/IME98.CFG`, and
`samples/AUTOEXEC.PC98.BAT` to the host share. Rename the sample to
`AUTOEXEC.BAT` when deploying it. The supplied AUTOEXEC installs the TSR when
the binary exists; foreground `/VERIFY` remains manual.

[`../samples/CONFIG.PC98.SYS`](../samples/CONFIG.PC98.SYS) is the matching
CONFIG.SYS example. A FreeDOS(98) boot may name this file `FDCONFIG.SYS`.
`IME98TSR.COM` needs no CONFIG.SYS device entry; the sample keeps the optional
`IME98.SYS` diagnostic line commented out.

Do not modify the HDI for an ordinary TSR test. If a test explicitly requires
an HDI change, stop np21w and create a backup first.

## `IME98.CFG`

The TSR reads configuration before becoming resident; interrupt handlers do
not access DOS files.

```ini
DATA_PORT=0030
STATUS_PORT=0032
MODE=02
COMMAND=27
TIMER_COUNT=0008
HOTKEY=SHIFT+SPACE
```

`TIMER_COUNT=0008h` is 19200 bps with np21w's verified 2.4576 MHz clock and
8251 x16 mode. Supported hotkeys are `SHIFT+SPACE`, `CTRL+SPACE`, and
`GRAPH+SPACE`. Space remains the primary key because the TSR can distinguish
and remove its verified BIOS queue word without consuming unrelated input.

Configuration keys are uppercase. Unknown values retain their defaults.

## Launch

The repository helper starts the bridge before np21w and copies current
binaries to the share:

```text
scripts\start-pc98-test.bat
```

For manual launch, start the bridge in Named Pipe client mode:

```text
ImeDosBridge.exe --pipe NP2-NamedPipe --pipe-client
```

## Optional debug pipe

Only the explicit `make pc98-tsr-debug pc98-debug` builds use PC-9861K CH1
through np21w COM2. They produce `IME98TSD.COM` and `IME98DBG.COM`; rename them
to the normal guest filenames when deploying manually. The release binaries
do not initialize or write to this second serial device.

Configure the development-only pipe as follows:

```text
Type:        PIPE
Pipe Name:   NP2-ImeDebug
Server Name: .
```

The bridge option is `--debug-pipe NP2-ImeDebug`. Verified PC-9861K CH1 ports
are data `00B1h` and status/control `00B3h`. This channel produces
`pc98-debug.log`; it is not part of the production protocol.

## Test discipline

- Keyboard actions are performed physically by the tester.
- Do not use Windows synthetic key injection with np21w.
- Record np21w revision, DOS version, loaded configuration, and application.
- Test ordinary Space separately from the configured modifier+Space hotkey.
- Verify text, Enter, Backspace, cursor keys, repeated sends, OFF, timeout, and
  reconnect behavior.
