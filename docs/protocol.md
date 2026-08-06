# Common Communication Protocol

The transport is a byte stream. Receivers frame packets, reject invalid
lengths, verify CRC16, and resynchronize on the next valid magic sequence.

## Packet types

| Value | Name | Direction | Payload |
|---:|---|---|---|
| 1 | HELLO | DOS → host | platform, client version, encoding, capacity, injection mode |
| 2 | PING | either | empty |
| 3 | PONG | either | empty |
| 4 | OPEN_IME | DOS → host | empty |
| 5 | TEXT | host → DOS | committed CP932 bytes |
| 6 | TEXT_ACK | DOS → host | empty |
| 7 | CLOSE_IME | either | empty |
| 8 | KEY | host → DOS | one logical key byte |
| 9 | KEY_ACK | DOS → host | empty |

KEY values are `01h` Enter, `02h` Left, `03h` Right, `04h` Up, `05h` Down,
and `06h` Backspace. The PC-98 mapping is `1C0Dh`, `3B00h`, `3C00h`, `3A00h`,
`3D00h`, and `0E08h`, respectively. These values were checked against np21w
rev103's keyboard definitions.

## HELLO payload

The implemented PC-98 payload is:

```text
offset  size  value
0       1     platform: 01h (PC-98)
1       1     client version: 02h
2       6     "CP932\0"
8       2     maximum TEXT bytes, little-endian (0200h = 512)
10      1     injection mode: 02h (resident staged BIOS queue)
```

Injection mode `01h` identifies the foreground 16-word BIOS-queue client;
mode `02h` identifies the resident staged client. The bridge validates TEXT
length against the advertised capacity before sending.

## Wire format

All multibyte integers are little-endian.

```text
offset  size  field
0       2     magic: 49 44 ("ID")
2       1     protocol version: 01
3       1     packet type
4       2     sequence
6       2     payload length
8       n     payload
8+n     2     CRC16/IBM
```

CRC16 uses polynomial `A001h` and initial value `FFFFh`, and covers the header
and payload but not the CRC field. Maximum payload length is 512 bytes.

OPEN_IME establishes the sequence used by its matching TEXT or KEY and ACK.
The PC-98 TSR issues another OPEN_IME after each successful ACK while its IME
toggle remains enabled. CLOSE_IME cancels that session; the PC-98 TSR sends it
for its local hotkey, and the host may send it for the configured Close key.

## Invariants

- CP932 bytes are opaque to the protocol.
- Partial, oversized, bad-CRC, wrong-sequence, and unsupported packets are not
  committed to the keyboard queue.
- A sender waits for the matching ACK before reusing the input surface.
- Timeouts and disconnects return the PC-98 toggle to a safe OFF state.
