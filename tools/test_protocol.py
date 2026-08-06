import unittest

from protocol import HELLO, PING, Packet, StreamDecoder, TEXT, crc16, decode


class ProtocolTests(unittest.TestCase):
    def test_crc_is_deterministic(self):
        self.assertEqual(crc16(b"123456789"), 0x4B37)

    def test_round_trip(self):
        packet = Packet(HELLO, 7, b"PC98\x00CP932")
        self.assertEqual(decode(packet.encode()), packet)

    def test_stream_handles_fragmentation(self):
        wire = Packet(PING, 1).encode() + Packet(TEXT, 2, b"OK").encode()
        decoder = StreamDecoder()
        result = []
        for byte in wire:
            result.extend(decoder.feed(bytes([byte])))
        self.assertEqual(result, [Packet(PING, 1), Packet(TEXT, 2, b"OK")])

    def test_stream_skips_garbage_and_bad_crc(self):
        bad = bytearray(Packet(PING, 1).encode())
        bad[-1] ^= 0xFF
        good = Packet(TEXT, 2, b"OK").encode()
        self.assertEqual(StreamDecoder().feed(b"garbage" + bytes(bad) + good), [Packet(TEXT, 2, b"OK")])


if __name__ == "__main__":
    unittest.main()
