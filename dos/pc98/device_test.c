#include <stdint.h>

static const char device_name[] = "IME98$";
static unsigned char command_packet[5] = { 'I', '9', '8', 1, 1 };
static unsigned char status_packet[8];

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
    dos_putc(digits[value & 0x0f]);
}

static void dos_exit(unsigned char code)
{
    uint16_t ax = 0x4c00 | code;
    __asm__ volatile ("int $0x21" : : "a" (ax) : "cc");
    for (;;)
        ;
}

static int device_open(const char *name, uint16_t *handle)
{
    uint16_t ax = 0x3d02;
    uint16_t failed;

    __asm__ volatile ("int $0x21; sbbw %0, %0"
                      : "=r" (failed), "+a" (ax)
                      : "d" (name)
                      : "cc");
    *handle = ax;
    return !failed;
}

static int device_ioctl(uint16_t function, uint16_t handle,
                        unsigned char *buffer, uint16_t length,
                        uint16_t *transferred)
{
    uint16_t ax = function;
    uint16_t failed;

    __asm__ volatile ("int $0x21; sbbw %0, %0"
                      : "=r" (failed), "+a" (ax)
                      : "b" (handle), "c" (length), "d" (buffer)
                      : "cc");
    *transferred = ax;
    return !failed;
}

static void device_close(uint16_t handle)
{
    uint16_t ax = 0x3e00;
    __asm__ volatile ("int $0x21" : "+a" (ax) : "b" (handle) : "cc");
}

int main(void)
{
    uint16_t handle;
    uint16_t transferred;

    dos_print("IME98 device IOCTL test\r\n");
    if (!device_open(device_name, &handle)) {
        dos_print("FAIL: cannot open IME98$\r\n");
        dos_exit(1);
    }
    if (!device_ioctl(0x4403, handle, command_packet, sizeof(command_packet),
                      &transferred) || transferred != sizeof(command_packet)) {
        dos_print("FAIL: IOCTL output\r\n");
        device_close(handle);
        dos_exit(1);
    }
    if (!device_ioctl(0x4402, handle, status_packet, sizeof(status_packet),
                      &transferred) || transferred != sizeof(status_packet) ||
        status_packet[0] != 'I' || status_packet[1] != '9' ||
        status_packet[2] != '8' || status_packet[3] != 1 ||
        status_packet[4] == 0 || status_packet[5] != 1 ||
        status_packet[6] != 1) {
        dos_print("FAIL: IOCTL input\r\n");
        device_close(handle);
        dos_exit(1);
    }
    dos_print("PASS: version=");
    print_hex(status_packet[3]);
    dos_print(" open-count=");
    print_hex(status_packet[4]);
    dos_print(" command=");
    print_hex(status_packet[5]);
    dos_print("\r\n");
    command_packet[4] = 2;
    if (!device_ioctl(0x4403, handle, command_packet, sizeof(command_packet),
                      &transferred) || transferred != sizeof(command_packet)) {
        dos_print("FAIL: IOCTL session close\r\n");
        device_close(handle);
        dos_exit(1);
    }
    device_close(handle);
    dos_exit(0);
    return 0;
}
