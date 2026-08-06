# PC98IMEBridge Current Specification

## 1. Purpose and release scope

PC98IMEBridge lets a PC-98 application receive Japanese text committed by a Windows
IME. The product target is a resident PC-98 client running with np21w.

The supported release is emulator-qualified, not real-PC-98-qualified.

## 2. Components

```text
Windows host
  ImeDosBridge.exe     Windows Forms input, CP932 conversion, transport

PC-98
  IME98TSR.COM         primary resident client
  IME98.COM            foreground regression and verification client
  IME98.SYS            diagnostic character-device path
  IME98.CFG            COM1 and resident-hotkey configuration
```

Generated release files belong under `release/`; intermediate files belong
under `build/` or .NET `bin/`/`obj/`.

## 3. Supported environment

### PC-98 release target

- np21w rev103 (`np21x64w.exe`)
- FreeDOS(98) test image
- 16-bit real mode and CP932/Shift-JIS applications
- np21w normal COM1 connected to Named Pipe `NP2-NamedPipe` at 19200 bps

### Windows host

- Windows 10 or 11
- .NET 8 Windows Desktop runtime or compatible roll-forward runtime
- any Windows IME that commits Unicode text to the bridge input control

### Not yet qualified

- real PC-9801/PC-9821 hardware
- NEC MS-DOS variants
- physical RS-232C transport
- arbitrary PC-98 editors beyond the recorded FreeCOM and VZ tests

## 4. Architecture and boundaries

```text
DOS application
    ↕ BIOS keyboard input
IME98TSR.COM
    ↕ PC-98 COM1 / np21w Named Pipe
ImeDosBridge
    ↕ Windows IME
```

- `dos/common/` owns packet framing and CRC16.
- `dos/pc98/` owns PC-98 BIOS memory, interrupt, PIC, 8251, and injection code.
- `host/ImeDosBridge/` owns Unicode conversion, UI, focus, and host transport.
- CP932 is opaque on the wire. Conversion occurs only at the Windows boundary.

Platform values must be verified against primary documentation or the exact
emulator source before use. IBM PC values must not be assumed valid on PC-98.

## 5. PC-98 resident behavior

1. `IME98TSR.COM` reads `IME98.CFG` before becoming resident.
2. It refuses duplicate owned hooks. `/U` identifies the resident copy,
   requires direct ownership of both installed vectors, stops COM/PIC state,
   restores the previous vectors, and releases the resident DOS environment
   and PSP memory blocks.
3. The configured modifier+Space hotkey toggles the IME. Supported values are
   `SHIFT+SPACE`, `CTRL+SPACE`, and `GRAPH+SPACE`.
4. ON opens COM1, sends HELLO, waits for PONG, and sends OPEN_IME.
5. TEXT is staged in a 512-byte resident buffer and transferred to the 16-word
   PC-98 BIOS keyboard queue without overwriting existing input.
6. KEY supports Enter, Backspace, Left, Right, Up, and Down.
7. TEXT_ACK or KEY_ACK is sent only after complete queue insertion.
8. While ON, the TSR sends another OPEN_IME after each ACK.
9. Local hotkey, host CLOSE_IME, timeout, or disconnect returns to a safe OFF
   state and restores the saved PIC mask.

The resident worker must not call DOS services. It runs on a private resident
stack with `DS=SS=CS` and preserves the interrupted registers and FLAGS.

## 6. Windows bridge behavior

- Connect as a Named Pipe client for np21w COM1.
- Identify the platform and capacity from HELLO.
- Bring the bridge input to the foreground on OPEN_IME.
- Convert non-empty text to CP932 and send it with the OPEN sequence.
- Send editing keys independently from text.
- Accept only one outstanding TEXT or KEY until its ACK.
- With an empty field, Close sends CLOSE_IME and returns focus to np21w after
  the initiating Shift key is released.
- Reconnect after stream, pipe, or protocol failure.
- Show and record plain-language activity events. Normal logs must not expose
  raw bytes or numeric protocol fields.

Default shortcuts and configurable names are documented in
`host/ImeDosBridge/IMEBRIDGE.CFG`.

## 7. Protocol

The common wire format, HELLO capability layout, packet values, CRC16, KEY
mapping, sequence rules, and error invariants are normative in
[`docs/protocol.md`](docs/protocol.md). Protocol constants must remain
synchronized between `dos/include/protocol.h` and the host implementation.

## 8. Configuration

### `IME98.CFG`

```ini
DATA_PORT=0030
STATUS_PORT=0032
MODE=02
COMMAND=27
TIMER_COUNT=0008
HOTKEY=SHIFT+SPACE
```

The defaults are np21w normal COM1, not FIFO mode. Configuration is loaded only
at installation; restart/reinstall the TSR after changes.

### `IMEBRIDGE.CFG`

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

Bindings accept optional `Ctrl+`, `Shift+`, and `Alt+`; `NONE` disables one.
Restart the bridge after changes.

## 9. Safety requirements

- The PC-98 TSR build must use `-fno-common` and pass
  `tools/check_pc98_tsr_map.py`.
- Release `IME98TSR.COM` and `IME98.COM` builds must use only normal PC-98
  COM1. PC-9861K CH.1 logging is compiled only into the explicitly named
  development binaries `IME98TSD.COM` and `IME98DBG.COM`.
- Packet payload length is at most 512 bytes and every packet is CRC-checked.
- Ordinary Space must remain ordinary input; only configured modifier+Space is
  consumed as the hotkey.
- Existing BIOS queue contents must not be overwritten.
- COM1 interrupt ownership must be bounded to active protocol sessions.
- System port `35h` must not be changed by the TSR.
- `/U` must not free resident memory until no installed vector points into it.
- `/U` must refuse removal when either installed vector is no longer directly
  owned by the identified resident copy.
- Windows synthetic key injection must not be used for np21w tests.
- Routine emulator tests must not modify the HDI.

## 10. Acceptance checks

Automated:

```sh
make test
make pc98-tsr
make pc98 pc98-sys pc98-device-test
DOTNET_CLI_HOME=/tmp/pc98imebridge-dotnet-cli \
  ./.dotnet/dotnet build host/ImeDosBridge.sln -p:EnableWindowsTargeting=true
```

Manual np21w acceptance:

- ordinary Space and repeated hotkey ON/OFF
- Japanese TEXT displayed immediately without an extra physical key
- multiple sends in one ON session
- Enter, Backspace, and four cursor keys in FreeCOM/VZ
- empty-input Close and focus return to np21w
- host absence, timeout, restart, and reconnect without keyboard lock
- repeated install, `/U`, memory recovery, and reinstall without reboot

Remaining release work is tracked only in
[`docs/release-checklist.md`](docs/release-checklist.md) and
[`docs/test-matrix.md`](docs/test-matrix.md).
