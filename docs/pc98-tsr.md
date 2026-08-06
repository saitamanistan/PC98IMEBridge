# PC-98 TSR Design

TSR (Terminate and Stay Resident) is a DOS program that remains in memory
after its installer returns. `IME98TSR.COM` stays resident to detect the
configured hotkey, communicate with the Bridge, and inject received input.

`make pc98-tsr` builds `build/pc98/IME98TSR.COM`. The validated development
target is np21w rev103 with FreeDOS(98), COM1 at 19200 bps, and the
`NP2-NamedPipe` endpoint.

## User-visible behavior

- The configured modifier+Space hotkey toggles the resident IME. The default
  is Shift+Space; Ctrl+Space and Graph+Space are supported by `IME98.CFG`.
- ON connects to the bridge, completes HELLO/PONG, and sends OPEN_IME.
- The bridge sends CP932 TEXT or an editing KEY. The TSR stages the input into
  the PC-98 BIOS keyboard queue and acknowledges it.
- After an ACK the TSR sends another OPEN_IME while the toggle remains ON.
- The local hotkey or host CLOSE_IME turns the session OFF.
- A bounded response timeout closes COM1 and returns to OFF.
- `/U` requires direct ownership of both installed vectors, stops COM1 and
  restores the saved PIC mask, restores the previous vectors, and releases
  the resident environment and PSP memory blocks.

## Verified platform details

The implementation uses values checked against a local verification copy of
the np21w rev103 source:

- keyboard queue: physical `0000:0502h`–`0521h`
- queue head/tail/count: `0524h`, `0526h`, `0528h`
- raw key bitmap: `052Ah`
- modifier state: `053Ah` (Shift bit 0, Graph bit 3, Ctrl bit 4)
- Space scan code: `34h`
- COM1 data/status: `0030h`/`0032h` in normal non-FIFO mode
- COM1 receive interrupt: `INT 0Ch`, PIC IMR port `02h` bit 4

Alternative I/O values must be supplied through `IME98.CFG`; do not infer
PC-98 values from IBM PC hardware.

## Interrupt and worker model

FreeCOM does not call DOS idle `INT 28h` continuously while waiting for input.
np21w's active BIOS-emulation timer path also bypasses guest `INT 08h` and
`INT 1Ch`. The TSR therefore uses two active entries:

1. The `INT 18h` input hook observes modifier and Space state, consumes only
   the matching hotkey word, and preserves normal keyboard behavior.
2. The `INT 0Ch` COM1 hook captures receive bytes into a 128-byte resident
   ring, acknowledges the PIC, and runs a bounded protocol worker before
   returning.

Interrupt callers can have an arbitrary `SS`. C code receives near pointers to
stack locals, so every worker switches to a 1024-byte resident stack and sets
`DS=SS=CS`. It restores the interrupted stack, registers, and FLAGS on exit.
No worker calls DOS services.

## Hotkey suppression and ordinary Space

Both ordinary Space and modifier+Space return BIOS word `3420h`. The TSR checks
the configured raw modifier bit before suppressing that word. Without the
modifier, Space is returned unchanged.

When BIOS `INT 18h/AH=00h` returns the hotkey, that word has already left the
queue. The post-return handler toggles the IME and waits on the caller stack for
the next queue word. It uses `STI`/`HLT`; keyboard or COM1 receive interrupts
wake the wait. This avoids re-entering np21w's blocking BIOS path, which did not
reliably notice a queue word inserted by the serial worker.

The hotkey latch uses each newly consumed or returned Space word as evidence
of a new press. Repeated worker calls during one key hold do not retrigger it.

## Text and editing-key injection

The resident protocol advertises a 512-byte staged capacity and injection mode
`02h`. TEXT bytes are encoded as BIOS key words in batches of up to 16, allowing
the BIOS queue to drain between batches. CR/LF is normalized to one Enter word.

KEY packets map to verified PC-98 words:

| Logical key | BIOS word |
|---|---:|
| Enter | `1C0Dh` |
| Left | `3B00h` |
| Right | `3C00h` |
| Up | `3A00h` |
| Down | `3D00h` |
| Backspace | `0E08h` |

Existing queued input is never overwritten. ACK is sent only after the complete
TEXT or KEY has entered the BIOS queue.

## Serial ownership

The TSR does not rewrite system port `35h`, which also controls unrelated
hardware including the beeper. During an active protocol transaction it saves
the PIC mask, unmasks only COM1 bit 4, owns the receive interrupt, and restores
the original mask when the session closes.

The serial IRQ first stores received data, sends PIC EOI, then invokes the
bounded worker on the resident stack. Received text therefore progresses
without another physical PC-98 key event.

## Resident memory safety

All state referenced by installed interrupt handlers must remain below
`pc98_tsr_resident_end`. The TSR build uses `-fno-common`, emits a linker map,
and runs `tools/check_pc98_tsr_map.py` on every build to enforce that boundary.

The transient `/U` copy locates the resident segment through `INT 18h`, checks
the resident signature, and verifies that both `INT 18h` and `INT 0Ch` still
point directly to that same copy. It performs resident shutdown on the private
stack, restores both old vectors, and only then calls DOS memory-free services.
If another TSR has replaced either vector, removal is refused without changing
the vector chain.

## Verified emulator behavior

np21w rev103 with FreeDOS(98) verifies:

- repeated hotkey ON/OFF without leaked Space characters
- immediate ASCII and Japanese CP932 display without a wake-up key
- repeated TEXT sends in one ON session
- Enter, Backspace, and corrected PC-98 cursor-key mappings in VZ Editor
- repeated install/`/U` cycles restored the pre-install largest executable
  block; the final tested build recovered from 611,680 to 621,072 bytes
- `/U` refused removal when a later test TSR owned `INT 18h`

This is emulator evidence, not real-hardware qualification. Synthetic Windows
key injection is excluded from the test method.
