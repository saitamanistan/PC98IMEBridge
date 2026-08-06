#ifndef DEBUG_SERIAL_PC98_H
#define DEBUG_SERIAL_PC98_H

#if defined(PC98IMEBRIDGE_PC98_DEBUG_SERIAL)
void pc98_debug_open(void);
void pc98_debug_write(const char *text);
void pc98_debug_write_hex_byte(unsigned char value);
void pc98_debug_write_hex_word(unsigned short value);
int pc98_debug_read(unsigned char *value);
int pc98_debug_read_byte(void);
#else
#define pc98_debug_open() ((void)0)
#define pc98_debug_write(text) ((void)0)
#define pc98_debug_write_hex_byte(value) ((void)0)
#define pc98_debug_write_hex_word(value) ((void)0)
#define pc98_debug_read(value) (0)
#define pc98_debug_read_byte() (-1)
#endif

#endif
