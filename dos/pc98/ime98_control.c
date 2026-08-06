#include <stdint.h>

static const char device_name[] = "IME98$";
static unsigned char command_packet[5];
static uint16_t device_handle;
static uint16_t device_last_ax;
static int device_is_open;

static int device_ioctl(uint16_t function, unsigned char *buffer,
                        uint16_t length)
{
    uint16_t ax = function;
    uint16_t failed;

    __asm__ volatile ("int $0x21; sbbw %0, %0"
                      : "=r" (failed), "+a" (ax)
                      : "b" (device_handle), "c" (length), "d" (buffer)
                      : "cc");
    device_last_ax = ax;
    /* DOS returns the transferred byte count in AX, but a few PC-98 DOS
       device stacks do not preserve that value for IOCTL output.  Carry is
       the authoritative success indication; the driver validates the exact
       packet length before accepting it. */
    (void)length;
    return !failed;
}

int ime98_control_open(void)
{
    uint16_t ax = 0x3d02;
    uint16_t failed;

    /* This COM is linked without a C startup object, so BSS is not
       guaranteed to be zeroed by the DOS loader.  Establish the state
       explicitly before relying on the device handle. */
    device_is_open = 0;
    device_handle = 0;
    __asm__ volatile ("int $0x21; sbbw %0, %0"
                      : "=r" (failed), "+a" (ax)
                      : "d" (device_name)
                      : "cc");
    if (failed)
        return 0;
    device_handle = ax;
    device_is_open = 1;
    return 1;
}

int ime98_control_command(unsigned char command)
{
    uint16_t ax = 0x4403;
    uint16_t failed;

    if (!device_is_open)
        return 0;
    command_packet[0] = 'I';
    command_packet[1] = '9';
    command_packet[2] = '8';
    command_packet[3] = 1;
    command_packet[4] = command;
    __asm__ volatile ("int $0x21; sbbw %0, %0"
                      : "=r" (failed), "+a" (ax)
                      : "b" (device_handle), "c" ((uint16_t)sizeof(command_packet)),
                        "d" ((uint16_t)(uintptr_t)command_packet)
                      : "cc");
    device_last_ax = ax;
    return !failed;
}

uint16_t ime98_control_last_ax(void)
{
    return device_last_ax;
}

void ime98_control_close(void)
{
    uint16_t ax = 0x3e00;
    if (!device_is_open)
        return;
    __asm__ volatile ("int $0x21" : "+a" (ax) : "b" (device_handle) : "cc");
    device_is_open = 0;
}
