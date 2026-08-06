#ifndef IME_DOS_PROTOCOL_H
#define IME_DOS_PROTOCOL_H

#define IME_MAGIC0 0x49
#define IME_MAGIC1 0x44
#define IME_VERSION 1
#define IME_MAX_PAYLOAD 512

#define IME_HELLO 1
#define IME_PING 2
#define IME_PONG 3
#define IME_OPEN_IME 4
#define IME_TEXT 5
#define IME_TEXT_ACK 6
#define IME_CLOSE_IME 7
#define IME_KEY 8
#define IME_KEY_ACK 9

#define IME_KEY_ENTER 1
#define IME_KEY_LEFT 2
#define IME_KEY_RIGHT 3
#define IME_KEY_UP 4
#define IME_KEY_DOWN 5
#define IME_KEY_BACKSPACE 6

unsigned short ime_crc16(const unsigned char *data, unsigned short length);
unsigned short ime_packet_build(unsigned char *out, unsigned char type,
                                unsigned short sequence,
                                const unsigned char *payload,
                                unsigned short payload_length);
int ime_packet_parse(const unsigned char *packet, unsigned short packet_length,
                     unsigned char *type, unsigned short *sequence,
                     const unsigned char **payload,
                     unsigned short *payload_length);

#endif
