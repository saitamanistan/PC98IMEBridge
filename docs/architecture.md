# Architecture

## Runtime path

```text
PC-98 application
    ↕ PC-98 BIOS keyboard queue
IME98TSR.COM
    ↕ COM1 (19200 bps)
np21w Named Pipe: NP2-NamedPipe
    ↕
ImeDosBridge.exe
    ↕
Windows IME
```

The PC-98 TSR owns hotkey detection, protocol state, COM1 receive capture,
CP932 staging, and BIOS key injection. The Windows bridge owns the input UI,
Unicode-to-CP932 conversion, focus management, and transport lifecycle.

## Boundaries

- `dos/common/` contains framing and CRC16 code shared by the PC-98 clients.
- `dos/pc98/` contains PC-98 BIOS work-area, interrupt, 8251, PIC, and
  keyboard-queue behavior.
- `host/ImeDosBridge/` contains the Windows bridge and Named Pipe/TCP
  transports.

CP932 is opaque at the protocol layer. Unicode conversion happens only at the
Windows boundary. A TEXT payload is acknowledged only after its bytes have
been staged into the PC-98 BIOS keyboard queue.

## PC-98 execution model

The application calls BIOS `INT 18h` for keyboard input. The TSR uses that
entry to detect the configured modifier+Space hotkey and to preserve ordinary
Space behavior. COM1 receive interrupt `INT 0Ch` captures bytes into a
resident ring and runs a bounded worker on a private resident stack. This is
necessary because np21w's active BIOS-emulation timer path does not invoke the
guest `INT 08h`/`INT 1Ch` vectors while FreeCOM is waiting for input.

The worker never calls DOS services after installation. Configuration is read
before the TSR becomes resident.

## Non-product paths

`IME98.COM`, `IME98.SYS`, probe programs, and the PC-9861K debug pipe remain
useful regression and diagnostic paths. They are not the primary interactive
PC-98 input path.
