#include "protocol.h"

unsigned short ime_crc16(const unsigned char *data, unsigned short length)
{
    unsigned short crc = 0xFFFF;
    unsigned char bit;

    while (length--) {
        crc ^= *data++;
        for (bit = 0; bit < 8; ++bit)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

unsigned short ime_packet_build(unsigned char *out, unsigned char type,
                                unsigned short sequence,
                                const unsigned char *payload,
                                unsigned short payload_length)
{
    unsigned short i;
    unsigned short crc;

    if (payload_length > IME_MAX_PAYLOAD)
        return 0;
    out[0] = IME_MAGIC0;
    out[1] = IME_MAGIC1;
    out[2] = IME_VERSION;
    out[3] = type;
    out[4] = (unsigned char)sequence;
    out[5] = (unsigned char)(sequence >> 8);
    out[6] = (unsigned char)payload_length;
    out[7] = (unsigned char)(payload_length >> 8);
    for (i = 0; i < payload_length; ++i)
        out[8 + i] = payload[i];
    crc = ime_crc16(out, 8 + payload_length);
    out[8 + payload_length] = (unsigned char)crc;
    out[9 + payload_length] = (unsigned char)(crc >> 8);
    return 10 + payload_length;
}

int ime_packet_parse(const unsigned char *packet, unsigned short packet_length,
                     unsigned char *type, unsigned short *sequence,
                     const unsigned char **payload,
                     unsigned short *payload_length)
{
    unsigned short length;
    unsigned short expected;
    unsigned short actual;

    if (packet_length < 10 || packet[0] != IME_MAGIC0 || packet[1] != IME_MAGIC1 ||
        packet[2] != IME_VERSION)
        return 0;
    length = (unsigned short)packet[6] | ((unsigned short)packet[7] << 8);
    expected = 10 + length;
    if (length > IME_MAX_PAYLOAD || packet_length != expected)
        return 0;
    actual = (unsigned short)packet[8 + length] |
             ((unsigned short)packet[9 + length] << 8);
    if (actual != ime_crc16(packet, 8 + length))
        return 0;
    *type = packet[3];
    *sequence = (unsigned short)packet[4] | ((unsigned short)packet[5] << 8);
    *payload = packet + 8;
    *payload_length = length;
    return 1;
}
