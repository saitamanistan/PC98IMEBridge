# Windows IME Bridge

`ImeDosBridge` is the .NET 8 Windows Forms companion for the PC-98 client. It
accepts the framed protocol over a Windows Named Pipe, identifies client
capabilities from HELLO, converts committed Windows text to CP932, and sends it
after OPEN_IME. TCP remains available only for host-side protocol diagnostics.

## PC-98 launch

np21w exposes COM1 as a Named Pipe server, so the bridge must use client mode:

```text
ImeDosBridge.exe --pipe NP2-NamedPipe --pipe-client
```

Development diagnostics may add:

```text
--debug-pipe NP2-ImeDebug
```

Use this option with the explicit DOS debug binaries `IME98TSD.COM` or
`IME98DBG.COM`. Release `IME98TSR.COM` and `IME98.COM` do not access CH.1 and
need only the main `--pipe` connection.

The debug pipe is independent of the protocol transport and is not used by
normal release binaries.

## UI behavior

- OPEN_IME brings the bridge input field to the foreground.
- Enter sends non-empty CP932 text.
- With an empty input field, Enter, Backspace, and cursor keys are sent as KEY
  messages.
- Esc clears non-empty local text. With an empty field it sends CLOSE_IME.
- CLOSE_IME returns focus to the `np21x64w` window after the corresponding
  host-visible modifier key is released. Only `Shift` and `Ctrl` have Windows
  keys that can be awaited; `GRAPH+SPACE` has no Windows modifier, so no key-up
  wait is performed for it.
- The bridge waits for TEXT_ACK or KEY_ACK before accepting another send.
- The `Activity` checkbox expands or hides a plain-language activity history.
  It is hidden by default.
- Editing keys are sent from the physical keyboard. The UI does not duplicate
  Enter, Backspace, or cursor keys as software buttons.

## Configuration

`IMEBRIDGE.CFG` is loaded from `AppContext.BaseDirectory` (beside the running
DLL or executable). Defaults are copied by the project build.

```ini
SEND=ENTER
CLOSE=ESCAPE
REMOTE_ENTER=ENTER
REMOTE_BACKSPACE=BACKSPACE
REMOTE_LEFT=LEFT
REMOTE_RIGHT=RIGHT
REMOTE_UP=UP
REMOTE_DOWN=DOWN
```

Values accept WinForms key names with optional `Ctrl+`, `Shift+`, and `Alt+`
modifiers. `NONE` disables a shortcut. Invalid lines are ignored and retain
the built-in default.

## Logs

`bridge-status.log` records the same plain-language activity history beside the
application. Raw bytes and numeric protocol fields are not logged. Development
builds started with `--debug-pipe` also write `pc98-debug.log`.

The bridge supports one client at a time and reconnects after Named Pipe,
stream, or protocol errors.
