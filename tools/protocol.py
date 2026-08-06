"""Wire codec for the DOS IME Bridge protocol."""

from __future__ import annotations

import struct
from dataclasses import dataclass

MAGIC = b"ID"
VERSION = 1
MAX_PAYLOAD = 512
HEADER = struct.Struct("<2sBBHH")
CRC = struct.Struct("<H")

HELLO = 1
PING = 2
PONG = 3
OPEN_IME = 4
TEXT = 5
TEXT_ACK = 6


def crc16(data: bytes) -> int:
    value = 0xFFFF
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ 0xA001 if value & 1 else value >> 1
    return value


@dataclass(frozen=True)
class Packet:
    kind: int
    sequence: int
    payload: bytes = b""

    def encode(self) -> bytes:
        if not 0 <= self.kind <= 255:
            raise ValueError("packet type out of range")
        if not 0 <= self.sequence <= 0xFFFF:
            raise ValueError("sequence out of range")
        if len(self.payload) > MAX_PAYLOAD:
            raise ValueError("payload exceeds negotiated maximum")
        header = HEADER.pack(MAGIC, VERSION, self.kind, self.sequence, len(self.payload))
        return header + self.payload + CRC.pack(crc16(header + self.payload))


def decode(data: bytes) -> Packet:
    if len(data) < HEADER.size + CRC.size:
        raise ValueError("short packet")
    magic, version, kind, sequence, length = HEADER.unpack_from(data)
    if magic != MAGIC or version != VERSION:
        raise ValueError("invalid packet header")
    if length > MAX_PAYLOAD or len(data) != HEADER.size + length + CRC.size:
        raise ValueError("invalid packet length")
    payload = data[HEADER.size : HEADER.size + length]
    actual = CRC.unpack_from(data, HEADER.size + length)[0]
    if actual != crc16(data[: HEADER.size + length]):
        raise ValueError("CRC mismatch")
    return Packet(kind, sequence, payload)


class StreamDecoder:
    def __init__(self) -> None:
        self.buffer = bytearray()

    def feed(self, data: bytes) -> list[Packet]:
        self.buffer.extend(data)
        packets: list[Packet] = []
        while len(self.buffer) >= HEADER.size + CRC.size:
            if self.buffer[:2] != MAGIC:
                del self.buffer[0]
                continue
            length = struct.unpack_from("<H", self.buffer, 6)[0]
            total = HEADER.size + length + CRC.size
            if length > MAX_PAYLOAD:
                del self.buffer[0]
                continue
            if len(self.buffer) < total:
                break
            candidate = bytes(self.buffer[:total])
            try:
                packets.append(decode(candidate))
            except ValueError:
                del self.buffer[0]
                continue
            del self.buffer[:total]
        return packets
