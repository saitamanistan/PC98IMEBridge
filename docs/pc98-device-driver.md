# PC-98 Diagnostic Device Driver

`IME98.SYS` is a safe diagnostic skeleton retained for regression testing. The
supported interactive client is `IME98TSR.COM`. The device driver is not used
for normal PC98IMEBridge operation.

`make pc98-sys` builds `build/pc98/IME98.SYS`. The driver implements DOS device
initialization, Open, Close, IOCTL Input (`INT 21h/AX=4402h`), and IOCTL Output
(`4403h`). It does not hook hardware interrupts or modify the keyboard BIOS.

`make pc98-device-test` builds `build/pc98/IME98DEV.COM`. The diagnostic opens
`IME98$`, sends a side-effect-free IOCTL request, reads status, and closes the
handle.

## IOCTL v1

IOCTL Output accepts five bytes: ASCII `I98`, API version `01h`, and a command.
Commands are `00h` PING, `01h` session start, and `02h` session end. IOCTL Input
returns eight bytes:

| Offset | Meaning |
|---:|---|
| 0-2 | ASCII `I98` |
| 3 | API version (`01h`) |
| 4 | open count |
| 5 | last accepted command |
| 6 | session state (`00h` or `01h`) |
| 7 | reserved (`00h`) |

Short buffers, an invalid signature, and unsupported commands return a DOS
device error. Emulator testing confirmed driver loading, normal FreeCOM input,
and `IME98DEV.COM` status output. Do not edit the boot HDI merely to repeat this
test; use a disposable copy with a verified backup when a boot-time driver test
is specifically required.
