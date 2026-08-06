using System.Buffers.Binary;

namespace ImeDosBridge;

public readonly record struct Packet(byte Type, ushort Sequence, byte[] Payload)
{
    public const int MaxPayload = 512;
    public const byte Version = 1;
    private const byte Magic0 = 0x49;
    private const byte Magic1 = 0x44;

    public byte[] Encode()
    {
        if (Payload.Length > MaxPayload)
            throw new InvalidDataException("Payload exceeds 512 bytes.");

        var result = new byte[10 + Payload.Length];
        result[0] = Magic0;
        result[1] = Magic1;
        result[2] = Version;
        result[3] = Type;
        BinaryPrimitives.WriteUInt16LittleEndian(result.AsSpan(4), Sequence);
        BinaryPrimitives.WriteUInt16LittleEndian(result.AsSpan(6), (ushort)Payload.Length);
        Payload.CopyTo(result, 8);
        BinaryPrimitives.WriteUInt16LittleEndian(result.AsSpan(8 + Payload.Length), Crc16(result.AsSpan(0, 8 + Payload.Length)));
        return result;
    }

    public static Packet Decode(ReadOnlySpan<byte> data)
    {
        if (data.Length < 10 || data[0] != Magic0 || data[1] != Magic1 || data[2] != Version)
            throw new InvalidDataException("Invalid packet header.");
        var length = BinaryPrimitives.ReadUInt16LittleEndian(data.Slice(6, 2));
        if (length > MaxPayload || data.Length != 10 + length)
            throw new InvalidDataException("Invalid packet length.");
        var expected = BinaryPrimitives.ReadUInt16LittleEndian(data.Slice(8 + length, 2));
        if (expected != Crc16(data[..(8 + length)]))
            throw new InvalidDataException("CRC mismatch.");
        return new Packet(data[3], BinaryPrimitives.ReadUInt16LittleEndian(data.Slice(4, 2)), data.Slice(8, length).ToArray());
    }

    private static ushort Crc16(ReadOnlySpan<byte> data)
    {
        ushort crc = 0xFFFF;
        foreach (var value in data)
        {
            crc ^= value;
            for (var bit = 0; bit < 8; bit++)
                crc = (crc & 1) != 0 ? (ushort)((crc >> 1) ^ 0xA001) : (ushort)(crc >> 1);
        }
        return crc;
    }
}
