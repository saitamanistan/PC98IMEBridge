#include <stdint.h>
#include "protocol.h"

int pc98_serial_load_config(void);
int pc98_serial_open(void);
void pc98_serial_close(void);
int pc98_serial_write(const unsigned char *data, unsigned short length);

static unsigned char port_in(unsigned short port)
{
    unsigned char value;
    __asm__ volatile ("inb %1, %0" : "=a" (value) : "d" (port));
    return value;
}

static void putc_dos(unsigned char value)
{
    uint16_t ax = 0x0200 | value;
    __asm__ volatile ("int $0x21" : "+a" (ax) : "d" ((uint16_t)value) : "cc");
}

static void print(const char *text)
{
    while (*text)
        putc_dos((unsigned char)*text++);
}

static void print_hex(unsigned char value)
{
    static const char digits[] = "0123456789ABCDEF";
    putc_dos(digits[value >> 4]);
    putc_dos(digits[value & 0x0f]);
}

static void delay(void)
{
    volatile unsigned long count = 30000UL;
    while (count--)
        ;
}

static uint16_t log_handle;

static int log_open(void)
{
    static const char name[] = "Z:\\CSTAT.LOG";
    uint16_t ax = 0x3c00;
    uint16_t cx = 0;
    uint16_t dx = (uint16_t)(uintptr_t)name;
    uint16_t failed;

    __asm__ volatile ("int $0x21; sbbw %0, %0"
                      : "=r" (failed), "+a" (ax)
                      : "c" (cx), "d" (dx) : "cc");
    if (failed)
        return 0;
    log_handle = ax;
    return 1;
}

static void log_write(const char *text, unsigned short length)
{
    uint16_t ax = 0x4000;
    uint16_t bx = log_handle;
    uint16_t cx = length;
    uint16_t dx = (uint16_t)(uintptr_t)text;
    uint16_t failed;

    if (log_handle == 0xffff)
        return;
    __asm__ volatile ("int $0x21; sbbw %0, %0"
                      : "=r" (failed), "+a" (ax)
                      : "b" (bx), "c" (cx), "d" (dx) : "cc");
    (void)failed;
}

static void log_line(const char *text)
{
    unsigned short length = 0;
    while (text[length])
        ++length;
    log_write(text, length);
}

static void log_status(unsigned char status, int with_data, unsigned char data)
{
    char line[32];
    unsigned char position = 0;
    static const char digits[] = "0123456789ABCDEF";

    line[position++] = 'S'; line[position++] = 'T'; line[position++] = 'A';
    line[position++] = 'T'; line[position++] = 'U'; line[position++] = 'S';
    line[position++] = '=';
    line[position++] = digits[status >> 4];
    line[position++] = digits[status & 0x0f];
    if (with_data) {
        line[position++] = ' ';
        line[position++] = 'D'; line[position++] = 'A'; line[position++] = 'T';
        line[position++] = 'A'; line[position++] = '=';
        line[position++] = digits[data >> 4];
        line[position++] = digits[data & 0x0f];
    }
    line[position++] = '\r';
    line[position++] = '\n';
    log_write(line, position);
}

int main(void)
{
    static const unsigned char hello_payload[] =
        { 1, 1, 'C', 'P', '9', '3', '2', 0 };
    unsigned char hello[32];
    unsigned short hello_length;
    unsigned char status;
    unsigned char sample;
    unsigned char i;

    log_handle = 0xffff;
    log_open();
    log_line("PC-98 RS-232C status probe\r\n");
    print("PC-98 RS-232C status probe\r\n");
    pc98_serial_load_config();
    if (!pc98_serial_open()) {
        print("OPEN FAILED\r\n");
        return 1;
    }
    print("INITIAL STATUS=");
    status = port_in(0x132);
    print_hex(status);
    print("\r\n");
    log_line("INITIAL STATUS=");
    log_status(status, 0, 0);
    hello_length = ime_packet_build(hello, IME_HELLO, 1,
                                    hello_payload, sizeof(hello_payload));
    if (!pc98_serial_write(hello, hello_length)) {
        print("HELLO WRITE FAILED\r\n");
        log_line("HELLO WRITE FAILED\r\n");
    } else {
        print("HELLO SENT\r\n");
        log_line("HELLO SENT\r\n");
    }

    for (i = 0; i < 32; ++i) {
        status = port_in(0x132);
        print("STATUS=");
        print_hex(status);
        if (status & 0x02) {
            sample = port_in(0x130);
            print(" DATA=");
            print_hex(sample);
        }
        print("\r\n");
        log_status(status, (status & 0x02) != 0, sample);
        delay();
    }
    pc98_serial_close();
    print("DONE\r\n");
    log_line("DONE\r\n");
    return 0;
}
