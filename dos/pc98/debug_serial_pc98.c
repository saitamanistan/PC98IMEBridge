#include <stdint.h>
#include "debug_serial_pc98.h"

#define DEBUG_DATA_PORT 0x00b1
#define DEBUG_STATUS_PORT 0x00b3

static unsigned char port_in(unsigned short port)
{
    unsigned char value;
    __asm__ volatile ("inb %1, %0" : "=a" (value) : "d" (port));
    return value;
}

static void port_out(unsigned short port, unsigned char value)
{
    __asm__ volatile ("outb %0, %1" : : "a" (value), "d" (port));
}

void pc98_debug_open(void)
{
    volatile unsigned long settle = 1000000UL;
    /* np21w PC-9861K CH1: B1h=data, B3h=status/control. */
    port_out(DEBUG_STATUS_PORT, 0x00);
    port_out(DEBUG_STATUS_PORT, 0x00);
    port_out(DEBUG_STATUS_PORT, 0x00);
    port_out(DEBUG_STATUS_PORT, 0x40);
    port_out(DEBUG_STATUS_PORT, 0x02);
    port_out(DEBUG_STATUS_PORT, 0x27);
    /* np21w recreates the PC-9861K pipe once during machine reset. */
    while (settle--)
        ;
}

void pc98_debug_write(const char *text)
{
    unsigned short budget = 128;
    while (*text && budget--) {
        port_out(DEBUG_DATA_PORT, (unsigned char)*text++);
    }
}

void pc98_debug_write_hex_byte(unsigned char value)
{
    static const char digits[] = "0123456789ABCDEF";
    port_out(DEBUG_DATA_PORT, digits[value >> 4]);
    port_out(DEBUG_DATA_PORT, digits[value & 0x0f]);
    port_out(DEBUG_DATA_PORT, '\r');
    port_out(DEBUG_DATA_PORT, '\n');
}

void pc98_debug_write_hex_word(unsigned short value)
{
    static const char digits[] = "0123456789ABCDEF";
    port_out(DEBUG_DATA_PORT, digits[(value >> 12) & 0x0f]);
    port_out(DEBUG_DATA_PORT, digits[(value >> 8) & 0x0f]);
    port_out(DEBUG_DATA_PORT, digits[(value >> 4) & 0x0f]);
    port_out(DEBUG_DATA_PORT, digits[value & 0x0f]);
    port_out(DEBUG_DATA_PORT, '\r');
    port_out(DEBUG_DATA_PORT, '\n');
}

int pc98_debug_read(unsigned char *value)
{
    if (!(port_in(DEBUG_STATUS_PORT) & 0x02))
        return 0;
    *value = port_in(DEBUG_DATA_PORT);
    return 1;
}

int pc98_debug_read_byte(void)
{
    if (!(port_in(DEBUG_STATUS_PORT) & 0x02))
        return -1;
    return port_in(DEBUG_DATA_PORT);
}
