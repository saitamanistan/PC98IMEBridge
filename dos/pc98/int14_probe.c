#include <stdint.h>

static void putc_dos(unsigned char c)
{
    uint16_t ax = 0x0200 | c;
    __asm__ volatile ("int $0x21" : "+a" (ax) : "d" ((uint16_t)c) : "cc");
}

static void print(const char *s)
{
    while (*s)
        putc_dos((unsigned char)*s++);
}

static void hex(unsigned char value)
{
    static const char digits[] = "0123456789ABCDEF";
    putc_dos(digits[(value >> 4) & 15]);
    putc_dos(digits[value & 15]);
}

int main(void)
{
    uint16_t ax;
    uint16_t dx = 0;

    print("INT 14h COM1 probe\r\n");
    ax = 0x00a3;
    __asm__ volatile ("int $0x14" : "+a" (ax) : "d" (dx) : "cc");
    print("INIT AX=");
    hex((unsigned char)(ax >> 8));
    putc_dos(' ');
    hex((unsigned char)ax);
    print("\r\n");

    ax = 0x0300;
    __asm__ volatile ("int $0x14" : "+a" (ax) : "d" (dx) : "cc");
    print("STAT AH=");
    hex((unsigned char)(ax >> 8));
    print(" AL=");
    hex((unsigned char)ax);
    print("\r\n");

    ax = 0x0155;
    __asm__ volatile ("int $0x14" : "+a" (ax) : "d" (dx) : "cc");
    print("SEND AH=");
    hex((unsigned char)(ax >> 8));
    print("\r\n");
    return 0;
}
