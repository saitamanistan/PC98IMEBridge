#include <stdint.h>

static void dos_putc(unsigned char value)
{
    uint16_t ax = 0x0200 | value;
    uint16_t dx = value;
    __asm__ volatile ("int $0x21" : "+a" (ax) : "d" (dx) : "cc");
}

static void dos_print(const char *text)
{
    while (*text)
        dos_putc((unsigned char)*text++);
}

static void print_hex(unsigned char value)
{
    static const char digits[] = "0123456789ABCDEF";
    dos_putc(digits[value >> 4]);
    dos_putc(digits[value & 0x0F]);
}

static void dos_exit(void)
{
    uint16_t ax = 0x4C00;
    __asm__ volatile ("int $0x21" : : "a" (ax) : "cc");
    for (;;)
        ;
}

int main(void)
{
    uint16_t ax;
    uint16_t bx;
    unsigned char key;
    unsigned char scan;
    unsigned char modifiers;

    dos_print("PC-98 key probe: press Shift+Space; Esc exits\r\n");
    for (;;) {
        ax = 0x0100;
        __asm__ volatile ("int $0x18" : "+a" (ax), "=b" (bx) : : "cc");
        if (!bx)
            continue;

        ax = 0x0700;
        bx = 0;
        __asm__ volatile ("int $0x18" : "+a" (ax), "+b" (bx) : : "cc");
        key = (unsigned char)ax;
        scan = (unsigned char)(ax >> 8);
        modifiers = (unsigned char)bx;

        dos_print("JIS=");
        print_hex(key);
        dos_print(" SCAN=");
        print_hex(scan);
        dos_print(" MOD=");
        print_hex(modifiers);
        if (key == 0x20 && (modifiers & 0x01))
            dos_print(" SHIFT+SPACE DETECTED");
        dos_print("\r\n");

        if (key == 0x1B)
            dos_exit();
    }
    return 0;
}
