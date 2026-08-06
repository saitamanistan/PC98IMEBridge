# PC-98 Text and Key Injection

The TSR stages received input and writes it to the verified np21w BIOS keyboard
queue when space is available. It never overwrites existing queued input. A
small INT 18h worker hook keeps delivery moving even when the foreground
application is waiting for keyboard input.

The queue layout was verified against np21w rev103 source: data words occupy
physical `0000:0502` through `0521`, head and tail are at `0524h` and `0526h`,
and the count is at `0528h`. Each word stores the character byte in the low byte
and the scan code in the high byte. These are emulator-specific findings, not
assumptions about every PC-98 BIOS.

TEXT payloads preserve CP932 bytes. Enter is injected as character `0Dh` with
scan `1Ch`; CRLF is normalized to one Enter. Backspace uses `08h/0Eh`, Tab uses
`09h/0Fh`, and Esc uses `1Bh/00h`. Arrow keys arrive as protocol `KEY` messages
and are mapped to the verified PC-98 scan codes.

np21w with FreeDOS(98) has demonstrated immediate ASCII and Japanese input in
FreeCOM and interactive Japanese/editing-key input in VZ Editor. Long input is
drained in bounded chunks through the resident staging buffer. Other DOS
versions, applications, physical RS-232C, and real PC-98 hardware remain
qualification gaps.

The foreground `IME98.COM /VERIFY` program remains a non-resident diagnostic for
queue insertion and recovery. Product behavior and resident constraints are
documented in [`pc98-tsr.md`](pc98-tsr.md).
