#include <stdint.h>

#define PC98_CONFIG_READ_SIZE 64
#define PC98_CONFIG_LINE_SIZE 128

static int serial_opened;
volatile unsigned short pc98_serial_data_port = 0x0030;
volatile unsigned short pc98_serial_status_port = 0x0032;
static unsigned char serial_mode = 0x02;
static unsigned char serial_command = 0x27;
static unsigned short serial_timer_count = 0x0008;
static const char *serial_error = "no error";
volatile unsigned char pc98_hotkey_modifier_mask = 0x01;

void pc98_hotkey_set_defaults(void)
{
    pc98_hotkey_modifier_mask = 0x01;
}

void pc98_hotkey_set_modifier(unsigned char mask)
{
    pc98_hotkey_modifier_mask = mask;
}

static unsigned char hex_value(unsigned char value)
{
    if (value >= '0' && value <= '9') return (unsigned char)(value - '0');
    if (value >= 'A' && value <= 'F') return (unsigned char)(value - 'A' + 10);
    if (value >= 'a' && value <= 'f') return (unsigned char)(value - 'a' + 10);
    return 0xff;
}

static int parse_hex(const unsigned char *text, unsigned short *value)
{
    unsigned short result = 0;
    unsigned char digit;
    unsigned char count = 0;
    while (*text == ' ' || *text == '\t') ++text;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
    while ((digit = hex_value(*text)) != 0xff && count < 4) {
        result = (unsigned short)((result << 4) | digit);
        ++text;
        ++count;
    }
    if (!count) return 0;
    *value = result;
    return 1;
}

static int config_key(const unsigned char *line, const char *key)
{
    while (*key && *line == (unsigned char)*key) { ++line; ++key; }
    return *key == 0 && *line == '=';
}

static int config_value(const unsigned char *text, const char *value)
{
    while (*value && *text == (unsigned char)*value) { ++text; ++value; }
    while (*text == ' ' || *text == '\t') ++text;
    return *value == 0 && *text == 0;
}

static void parse_config_line(const unsigned char *line)
{
    unsigned short value;
    while (*line == ' ' || *line == '\t') ++line;
    if (!*line || *line == ';' || *line == '#') return;
    if (config_key(line, "DATA_PORT") && parse_hex(line + 10, &value))
        pc98_serial_data_port = value;
    else if (config_key(line, "STATUS_PORT") && parse_hex(line + 12, &value))
        pc98_serial_status_port = value;
    else if (config_key(line, "MODE") && parse_hex(line + 5, &value))
        serial_mode = (unsigned char)value;
    else if (config_key(line, "COMMAND") && parse_hex(line + 8, &value))
        serial_command = (unsigned char)value;
    else if (config_key(line, "TIMER_COUNT") && parse_hex(line + 12, &value))
        serial_timer_count = value;
    else if (config_key(line, "HOTKEY")) {
        const unsigned char *hotkey = line + 7;
        while (*hotkey == ' ' || *hotkey == '\t') ++hotkey;
        if (config_value(hotkey, "SHIFT+SPACE"))
            pc98_hotkey_set_modifier(0x01);
        else if (config_value(hotkey, "GRAPH+SPACE"))
            pc98_hotkey_set_modifier(0x08);
        else if (config_value(hotkey, "CTRL+SPACE"))
            pc98_hotkey_set_modifier(0x10);
    }
}

static int dos_open_readonly(const char *name)
{
    uint16_t ax = 0x3d00;
    uint16_t dx = (uint16_t)(uintptr_t)name;
    uint16_t failed;
    __asm__ volatile ("int $0x21; sbbw %0, %0"
                      : "=r" (failed), "+a" (ax) : "d" (dx) : "cc");
    return failed ? -1 : (int)ax;
}

static unsigned short dos_read(uint16_t handle, void *buffer, unsigned short length)
{
    uint16_t ax = 0x3f00;
    uint16_t failed;
    __asm__ volatile ("int $0x21; sbbw %0, %0"
                      : "=r" (failed), "+a" (ax)
                      : "b" (handle), "c" (length),
                        "d" ((uint16_t)(uintptr_t)buffer) : "cc");
    return failed ? 0 : ax;
}

static void dos_close(uint16_t handle)
{
    uint16_t ax = 0x3e00;
    __asm__ volatile ("int $0x21" : "+a" (ax) : "b" (handle) : "cc");
}

int pc98_serial_load_config(void)
{
    static const char name[] = "IME98.CFG";
    unsigned char read_buffer[PC98_CONFIG_READ_SIZE];
    unsigned char line[PC98_CONFIG_LINE_SIZE];
    unsigned short length;
    unsigned short position;
    unsigned short line_length = 0;
    unsigned char line_overflow = 0;
    unsigned char line_started = 0;
    unsigned char line_comment = 0;
    int handle = dos_open_readonly(name);
    if (handle < 0) return 0;

    for (;;) {
        length = dos_read((uint16_t)handle, read_buffer, sizeof(read_buffer));
        if (!length)
            break;
        for (position = 0; position < length; ++position) {
            unsigned char value = read_buffer[position];
            if (value == '\r' || value == '\n') {
                if (!line_overflow && !line_comment) {
                    line[line_length] = 0;
                    parse_config_line(line);
                }
                line_length = 0;
                line_overflow = 0;
                line_started = 0;
                line_comment = 0;
            } else if (!line_overflow && !line_comment) {
                if (!line_started) {
                    if (value == ' ' || value == '\t')
                        continue;
                    line_started = 1;
                    if (value == ';' || value == '#') {
                        line_comment = 1;
                        continue;
                    }
                }
                if (line_length + 1 < sizeof(line))
                    line[line_length++] = value;
                else
                    line_overflow = 1;
            }
        }
    }
    dos_close((uint16_t)handle);

    if (line_length && !line_overflow && !line_comment) {
        line[line_length] = 0;
        parse_config_line(line);
    }
    return 1;
}

static __attribute__((noinline)) unsigned char port_in(unsigned short port)
{
    unsigned char value;
    __asm__ volatile ("inb %1, %0" : "=a" (value) : "d" (port));
    return value;
}

static __attribute__((noinline)) void port_out(unsigned short port,
                                                unsigned char value)
{
    __asm__ volatile ("outb %0, %1" : : "a" (value), "d" (port));
}

int pc98_serial_open(void)
{
    /* np21w's normal RS-232C registers are 30h/32h. 130h/132h are valid
       only when its optional FIFO mode is enabled. */
    port_out(pc98_serial_status_port, 0x00);
    port_out(pc98_serial_status_port, 0x00);
    port_out(pc98_serial_status_port, 0x00);
    port_out(pc98_serial_status_port, 0x40);
    port_out(pc98_serial_status_port, serial_mode);
    port_out(pc98_serial_status_port, serial_command);

    /* np21w polls its COM1 named pipe only from the RS-232C PIT channel
       callback. Program PIT channel 2 exactly as np21w's BIOS INT 19h does:
       control port 77h, then the low/high count at 75h. With the standard
       2.4576 MHz source and the 8251 x16 mode, count 8 is 19200 baud. */
    port_out(0x0077, 0xb6);
    port_out(0x0075, (unsigned char)serial_timer_count);
    port_out(0x0075, (unsigned char)(serial_timer_count >> 8));
    serial_opened = 1;
    serial_error = "no error";
    return 1;
}

void pc98_serial_close(void)
{
    serial_opened = 0;
}

const char *pc98_serial_device_name(void)
{
    return "PC-98 RS-232C (IME98.CFG)";
}

const char *pc98_serial_last_error(void)
{
    return serial_error;
}

unsigned char pc98_serial_status(void)
{
    return port_in(pc98_serial_status_port);
}

unsigned char pc98_serial_system_port(void)
{
    return port_in(0x0035);
}

unsigned char pc98_serial_pic_mask(void)
{
    return port_in(0x0002);
}

int pc98_serial_write(const unsigned char *data, unsigned short length)
{
    unsigned short i;
    unsigned long timeout;
    if (!serial_opened)
        return 0;
    for (i = 0; i < length; ++i) {
        timeout = 0x2000UL;
        while (!(port_in(pc98_serial_status_port) & 0x01) && --timeout)
            ;
        if (!timeout)
        {
            serial_error = "TX timeout";
            return 0;
        }
        port_out(pc98_serial_data_port, data[i]);
    }
    return 1;
}

int pc98_serial_read_byte(void)
{
    unsigned long timeout;
    if (!serial_opened)
        return -1;
    timeout = 0xffffUL;
    while (!(port_in(pc98_serial_status_port) & 0x02) && --timeout)
        ;
    if (!timeout)
    {
        serial_error = "RX timeout";
        return -1;
    }
    return port_in(pc98_serial_data_port);
}

int pc98_serial_try_read_byte(void)
{
    if (!serial_opened || !(port_in(pc98_serial_status_port) & 0x02))
        return -1;
    return port_in(pc98_serial_data_port);
}
