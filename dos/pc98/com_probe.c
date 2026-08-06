#include <stdint.h>

static const char names[][5] = { "COM1", "COM2", "COM3", "COM4" };

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

static void hex(uint16_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    putc_dos(digits[(value >> 12) & 15]);
    putc_dos(digits[(value >> 8) & 15]);
    putc_dos(digits[(value >> 4) & 15]);
    putc_dos(digits[value & 15]);
}

int main(void)
{
    unsigned char i;
    for (i = 0; i < 4; ++i) {
        uint16_t ax = 0x3D02;
        uint16_t failed;
        __asm__ volatile ("int $0x21; sbbw %0, %0"
                          : "=r" (failed), "+a" (ax)
                          : "d" ((uint16_t)(uintptr_t)names[i])
                          : "cc");
        print(names[i]);
        if (failed) {
            print(": FAIL AX=");
            hex(ax);
            print("\r\n");
        } else {
            uint16_t close_ax = 0x3E00;
            print(": OPEN handle=");
            hex(ax);
            print("\r\n");
            __asm__ volatile ("int $0x21" : "+a" (close_ax) : "b" (ax) : "cc");
        }
    }
    return 0;
}
