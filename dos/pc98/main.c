#include <stdint.h>
#include "protocol.h"
#include "debug_serial_pc98.h"

int pc98_serial_load_config(void);
int pc98_serial_open(void);
void pc98_serial_close(void);
int pc98_serial_write(const unsigned char *data, unsigned short length);
int pc98_serial_read_byte(void);
const char *pc98_serial_device_name(void);
const char *pc98_serial_last_error(void);
unsigned char pc98_serial_status(void);
int ime98_control_open(void);
int ime98_control_command(unsigned char command);
uint16_t ime98_control_last_ax(void);
void ime98_control_close(void);
int pc98_bios_inject_verify(const unsigned char *text, unsigned short length);
int pc98_bios_inject(const unsigned char *text, unsigned short length);

static uint16_t log_handle = 0xffff;
static unsigned char serial_rx_packet[522];
static unsigned short serial_rx_length;

static void log_open(void)
{
    static const char name[] = "Z:\\IME98.LOG";
    uint16_t ax = 0x3c00;
    __asm__ volatile ("int $0x21"
                      : "+a" (ax)
                      : "c" ((uint16_t)0), "d" ((uint16_t)(uintptr_t)name)
                      : "cc");
    log_handle = ax;
}

static void log_write(const char *text, unsigned short length)
{
    uint16_t ax = 0x4000;
    if (log_handle == 0xffff)
        return;
    __asm__ volatile ("int $0x21"
                      : "+a" (ax)
                      : "b" (log_handle), "c" (length),
                        "d" ((uint16_t)(uintptr_t)text)
                      : "cc");
}

static void log_line(const char *text)
{
    unsigned short length = 0;
    while (text[length])
        ++length;
    log_write(text, length);
    pc98_debug_write(text);
}

static void log_packet_prefix(const unsigned char *packet, unsigned short length)
{
    static const char digits[] = "0123456789ABCDEF";
    static char line[103];
    unsigned short position = 0;
    unsigned short i;
    if (length > 32)
        length = 32;
    line[position++] = 'R';
    line[position++] = 'X';
    line[position++] = ' ';
    for (i = 0; i < length; ++i) {
        line[position++] = digits[packet[i] >> 4];
        line[position++] = digits[packet[i] & 0x0f];
        line[position++] = ' ';
    }
    line[position++] = '\r';
    line[position++] = '\n';
    line[position] = 0;
    log_write(line, position);
    pc98_debug_write(line);
}

static void close_session(void)
{
    pc98_serial_close();
    ime98_control_command(2);
    ime98_control_close();
}

static void retry_delay(void)
{
    volatile unsigned long count = 60000UL;
    while (count--)
        ;
}

static void dos_print(const char *text)
{
    while (*text) {
        uint16_t ax = 0x0200 | (unsigned char)*text++;
        uint16_t dx = (unsigned char)text[-1];
        __asm__ volatile ("int $0x21" : "+a" (ax) : "d" (dx) : "cc");
    }
}

static void dos_print_hex(uint16_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    static char text[5];
    unsigned char i;
    for (i = 0; i < 4; ++i)
        text[i] = digits[(value >> (12 - i * 4)) & 0x0f];
    text[4] = 0;
    dos_print(text);
}

static void dos_exit(unsigned char code)
{
    uint16_t ax = 0x4C00 | code;
    __asm__ volatile ("int $0x21" : : "a" (ax) : "cc");
    for (;;)
        ;
}

static int read_packet(void)
{
    int received;
    unsigned char value;
    unsigned short count = 0;
    unsigned short expected = 0;
    unsigned short idle_reads = 0;
    unsigned short byte_reads = 0;

    /* Each COM read has a bounded hardware wait. Bound the number of
       consecutive empty reads as well so a disconnected host cannot hang
       the foreground client forever. A received byte resets the budget. */
    while (idle_reads < 4 && byte_reads < 128) {
        received = pc98_serial_read_byte();
        if (received < 0) {
            ++idle_reads;
            continue;
        }
        value = (unsigned char)received;
        ++byte_reads;
        idle_reads = 0;
        if (count == 0 && value != IME_MAGIC0)
            continue;
        if (count == 1 && value != IME_MAGIC1) {
            count = 0;
            continue;
        }
        serial_rx_packet[count++] = value;
        if (count == 8) {
            expected = 10 + (unsigned short)serial_rx_packet[6] +
                       ((unsigned short)serial_rx_packet[7] << 8);
            if (expected > 522)
                return 0;
        }
        if (expected && count == expected) {
            serial_rx_length = count;
            return 1;
        }
        if (count >= 522)
            return 0;
    }
    return 0;
}

static int exchange(unsigned char type, unsigned short sequence,
                    unsigned char *response_type, unsigned char *payload,
                    unsigned short *payload_length)
{
    static unsigned char packet[522];
    static const unsigned char *response_payload;
    unsigned short length;
    static unsigned short response_sequence;
    unsigned short i;

    length = ime_packet_build(packet, type, sequence, 0, 0);
    if (!pc98_serial_write(packet, length) || !read_packet() ||
        !ime_packet_parse(serial_rx_packet, serial_rx_length, response_type, &response_sequence,
                          &response_payload, payload_length) ||
        response_sequence != sequence)
        return 0;
    for (i = 0; i < *payload_length; ++i)
        payload[i] = response_payload[i];
    return 1;
}

static int hello_exchange(unsigned char *packet, unsigned short *length,
                          unsigned char *response_type,
                          const unsigned char **hello_payload)
{
    static unsigned short response_length;
    static unsigned short response_sequence;
    unsigned char attempt;

    for (attempt = 0; attempt < 4; ++attempt) {
        log_line("HELLO ATTEMPT\r\n");
        if (attempt) {
            pc98_serial_close();
            retry_delay();
            if (!pc98_serial_open())
                continue;
        }
        log_line("HELLO WRITE BEGIN\r\n");
        if (!pc98_serial_write(packet, *length)) {
            log_line("HELLO WRITE FAILED\r\n");
        } else {
            log_line("HELLO WRITE OK\r\n");
            log_line("HELLO READ BEGIN\r\n");
            if (!read_packet()) {
                log_line("HELLO READ TIMEOUT\r\n");
                pc98_debug_write("COM1 STATUS ");
                pc98_debug_write_hex_byte(pc98_serial_status());
            } else {
                response_length = serial_rx_length;
                log_line("HELLO READ OK\r\n");
                log_packet_prefix(serial_rx_packet, response_length);
                if (!ime_packet_parse(serial_rx_packet, response_length, response_type,
                                      &response_sequence, hello_payload,
                                      &response_length)) {
                    log_line("HELLO PARSE FAILED\r\n");
                } else if (response_sequence != 1 || *response_type != IME_PONG) {
                    log_line("HELLO RESPONSE INVALID\r\n");
                } else {
                    return 1;
                }
            }
        }
        log_line("HELLO RETRY\r\n");
        retry_delay();
    }
    log_line("HELLO FAILED\r\n");
    return 0;
}

static int string_equal(const char *left, const char *right)
{
    while (*left && *right) {
        unsigned char a = (unsigned char)*left++;
        unsigned char b = (unsigned char)*right++;
        if (a >= 'a' && a <= 'z') a -= 0x20;
        if (b >= 'a' && b <= 'z') b -= 0x20;
        if (a != b) return 0;
    }
    return *left == 0 && *right == 0;
}

int main(int argc, char **argv)
{
    static unsigned char hello[] = {
        0x01, 0x01, 'C', 'P', '9', '3', '2', 0,
        16, 0, /* foreground BIOS queue capacity, little endian */
        0x01   /* injection mode: PC-98 BIOS keyboard queue */
    };
    static unsigned char packet[522];
    static unsigned char response_type;
    static unsigned char response_payload[512];
    static const unsigned char *hello_payload;
    static unsigned short length;
    static unsigned short text_length;
    unsigned short sequence = 1;
    int verify_injection = argc > 1 && string_equal(argv[1], "/VERIFY");

    dos_print("PC-98 IME Bridge test client\r\n");
    pc98_debug_open();
    log_open();
    log_line("START\r\n");
    pc98_serial_load_config();
    log_line("CONFIG LOADED\r\n");
    if (!ime98_control_open()) {
        log_line("ERROR IME98 OPEN\r\n");
        dos_print("ERROR: cannot open IME98$\r\n");
        dos_exit(1);
    }
    if (!ime98_control_command(1)) {
        log_line("ERROR IME98 SESSION OPEN\r\n");
        dos_print("ERROR: IME98$ session command failed AX=");
        dos_print_hex(ime98_control_last_ax());
        dos_print("\r\n");
        close_session();
        dos_exit(1);
    }
    dos_print("IME98$ session opened\r\n");
    log_line("IME98 SESSION OPENED\r\n");
    if (!pc98_serial_open()) {
        log_line("ERROR SERIAL OPEN\r\n");
        dos_print("ERROR: cannot initialize configured RS-232C port\r\n");
        close_session();
        dos_exit(1);
    }
    dos_print("COM device opened: ");
    dos_print(pc98_serial_device_name());
    dos_print("\r\n");
    log_line("SERIAL OPENED\r\n");
    length = ime_packet_build(packet, IME_HELLO, sequence, hello, sizeof(hello));
    if (!hello_exchange(packet, &length, &response_type, &hello_payload)) {
        log_line("ERROR HELLO\r\n");
        dos_print("ERROR: HELLO failed: ");
        dos_print(pc98_serial_last_error());
        dos_print("\r\n");
        close_session();
        dos_exit(1);
    }
    dos_print("HELLO acknowledged\r\n");
    log_line("HELLO ACKNOWLEDGED\r\n");
    if (!exchange(IME_PING, 2, &response_type, response_payload, &length) ||
        response_type != IME_PONG) {
        log_line("ERROR PING\r\n");
        dos_print("ERROR: PING failed\r\n");
        close_session();
        dos_exit(1);
    }
    dos_print("PONG received\r\n");
    log_line("PONG RECEIVED\r\n");
    if (!exchange(IME_OPEN_IME, 3, &response_type, response_payload, &length) ||
        response_type != IME_TEXT) {
        log_line("ERROR OPEN_IME TEXT\r\n");
        dos_print("ERROR: TEXT failed\r\n");
        close_session();
        dos_exit(1);
    }
    text_length = length;
    log_line("TEXT RECEIVED\r\n");
    log_packet_prefix(response_payload, text_length);
    if (verify_injection) {
        int injection_result = pc98_bios_inject_verify(response_payload, length);
        if (injection_result > 0)
            log_line("BIOS INJECTION VERIFIED\r\n");
        else if (injection_result < 0)
            log_line("BIOS INJECTION SKIPPED: KEY QUEUE BUSY OR TEXT TOO LONG\r\n");
        else
            log_line("BIOS INJECTION VERIFY FAILED\r\n");
    } else
        log_line("BIOS INJECTION QUEUE MODE\r\n");
    packet[0] = 0;
    length = ime_packet_build(packet, IME_TEXT_ACK, 3, 0, 0);
    if (!pc98_serial_write(packet, length)) {
        log_line("ERROR TEXT ACK\r\n");
        dos_print("ERROR: TEXT_ACK send failed\r\n");
        close_session();
        dos_exit(1);
    }
    close_session();
    if (verify_injection)
        log_line("PASS\r\n");
    else if (pc98_bios_inject(response_payload, text_length) > 0)
        pc98_debug_write("BIOS TEXT QUEUED FOR PARENT\r\nPASS\r\n");
    else {
        pc98_debug_write("BIOS TEXT QUEUE FAILED\r\n");
        dos_exit(1);
    }
    dos_exit(0);
    return 0;
}
