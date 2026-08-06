# Test Matrix

| Area | Target | Current evidence | Release gap |
|---|---|---|---|
| Protocol | Python/common C | CRC, round trip, fragmented stream, bad CRC recovery | add message-semantic vectors for KEY |
| Host build | .NET 8 Windows | clean build with zero warnings | automated UI/pipe tests |
| PC-98 resident map | ia16 linker map | `-fno-common` and automated resident-range check | none for emulator build |
| PC-98 transport | np21w rev103 COM1 | HELLO/PONG, TEXT/ACK, repeat OPEN | reconnect automation |
| PC-98 text | FreeDOS(98) FreeCOM | ASCII and CP932 Japanese displayed immediately | more applications and long input |
| PC-98 editor | VZ Editor | Japanese text and editing-key path exercised interactively | repeatable scripted checklist |
| PC-98 keyboard | np21w | ordinary Space, hotkey toggle, Enter, Backspace, arrows | Ctrl/Graph hotkey variants |
| PC-98 lifecycle | np21w | two install/`/U` cycles fully restored memory; later `INT 18h` owner was refused | active-session `/U` and separate `INT 0Ch` conflict run |
| PC-98 hardware | PC-9801/9821 | none | full qualification |

Hardware evidence supplements emulator tests; it does not replace deterministic
protocol and map checks. Record emulator/hardware, DOS, application,
configuration, transport, and observed result for every manual test.
