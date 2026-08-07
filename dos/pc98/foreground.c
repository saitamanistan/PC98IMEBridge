#include <stdint.h>
#include "protocol.h"
#include "debug_serial_pc98.h"

int pc98_serial_open(void);
void pc98_serial_close(void);
int pc98_serial_write(const unsigned char *data, unsigned short length);
int pc98_serial_try_read_byte(void);
unsigned char pc98_serial_status(void);
unsigned char pc98_serial_system_port(void);
unsigned char pc98_serial_pic_mask(void);
int pc98_tsr_serial_try_read_byte(void);
void pc98_tsr_serial_capture_begin(void);
void pc98_tsr_serial_capture_end(void);
int pc98_poll_hotkey(void);
unsigned short pc98_bios_encode_key(const unsigned char *text,
                                    unsigned short length,
                                    unsigned short *input);
int pc98_bios_enqueue_word(unsigned short key);
int pc98_bios_discard_hotkey_space(void);

extern volatile unsigned short pc98_serial_irq_entry_count;
extern volatile unsigned short pc98_serial_irq_rx_ready_count;
extern volatile unsigned short pc98_serial_irq_stored_count;
extern volatile unsigned short pc98_input_hook_count;
extern volatile unsigned short pc98_input_ah00_count;
extern volatile unsigned short pc98_input_ah01_count;
extern volatile unsigned short pc98_input_ah02_count;
extern volatile unsigned short pc98_input_ah05_count;
extern volatile unsigned short pc98_input_other_count;

enum {
    LINK_CLOSED = 0,
    LINK_HELLO_WAIT = 1,
    LINK_READY = 2
};

static unsigned char link_state;
static unsigned char open_pending;
static unsigned char ime_active;
static unsigned char ime_enabled;
static unsigned short sequence;
static unsigned short open_sequence;
static unsigned short wait_ticks;
static unsigned char diagnostic_delay;
static unsigned short direct_poll_reads;

static unsigned char tx_packet[522];
static unsigned char rx_packet[522];
static unsigned short rx_count;
static unsigned short rx_expected;

static unsigned char pending_text[512];
static unsigned short pending_length;
static unsigned short pending_offset;
static unsigned short pending_sequence;
static unsigned short pending_key;
static unsigned char pending_key_active;

static void log_transport_diagnostics(void);

static int send_packet(unsigned char type, unsigned short seq,
                       const unsigned char *payload, unsigned short length)
{
    unsigned short packet_length = ime_packet_build(tx_packet, type, seq,
                                                     payload, length);
    return packet_length && pc98_serial_write(tx_packet, packet_length);
}

static void reset_link(void)
{
    pc98_tsr_serial_capture_end();
    pc98_serial_close();
    link_state = LINK_CLOSED;
    ime_active = 0;
    wait_ticks = 0;
    rx_count = 0;
    rx_expected = 0;
}

static void send_open_ime(void)
{
    open_sequence = ++sequence;
    if (send_packet(IME_OPEN_IME, open_sequence, 0, 0)) {
        ime_active = 1;
        open_pending = 0;
        wait_ticks = 0;
        pc98_debug_write("TSR OPEN_IME SENT\r\n");
    } else {
        reset_link();
        open_pending = 1;
    }
}

static void close_ime(void)
{
    if (link_state == LINK_READY)
        send_packet(IME_CLOSE_IME, open_sequence, 0, 0);
    if (link_state != LINK_CLOSED)
        reset_link();
    ime_enabled = 0;
    open_pending = 0;
    ime_active = 0;
    pending_length = 0;
    pending_offset = 0;
    pending_key_active = 0;
    wait_ticks = 0;
    pc98_debug_write("TSR IME OFF\r\n");
}

void pc98_prepare_uninstall(void)
{
    /* The transient /U copy calls this entry on the resident stack before
       restoring vectors.  close_ime sends a bounded best-effort CLOSE when a
       host session is ready, then always stops capture and restores the PIC. */
    close_ime();
}

static void handle_packet(void)
{
    unsigned char type;
    unsigned short packet_sequence;
    const unsigned char *payload;
    unsigned short payload_length;
    unsigned short i;

    if (!ime_packet_parse(rx_packet, rx_count, &type, &packet_sequence,
                          &payload, &payload_length))
        return;

    if (type == IME_PONG && link_state == LINK_HELLO_WAIT) {
        link_state = LINK_READY;
        wait_ticks = 0;
        pc98_debug_write("TSR HELLO ACKNOWLEDGED\r\n");
        if (open_pending)
            send_open_ime();
    } else if (type == IME_TEXT && ime_active &&
               packet_sequence == open_sequence && payload_length <= 512) {
        for (i = 0; i < payload_length; ++i)
            pending_text[i] = payload[i];
        pending_length = payload_length;
        pending_offset = 0;
        pending_sequence = packet_sequence;
        wait_ticks = 0;
        pc98_debug_write("TSR TEXT RECEIVED\r\n");
        log_transport_diagnostics();
    } else if (type == IME_KEY && ime_active &&
               packet_sequence == open_sequence && payload_length == 1) {
        switch (payload[0]) {
        case IME_KEY_ENTER:
            pending_key = 0x1c0d;
            break;
        case IME_KEY_LEFT:
            pending_key = 0x3b00;
            break;
        case IME_KEY_RIGHT:
            pending_key = 0x3c00;
            break;
        case IME_KEY_UP:
            pending_key = 0x3a00;
            break;
        case IME_KEY_DOWN:
            pending_key = 0x3d00;
            break;
        case IME_KEY_BACKSPACE:
            pending_key = 0x0e08;
            break;
        default:
            return;
        }
        pending_key_active = 1;
        pending_sequence = packet_sequence;
        wait_ticks = 0;
        pc98_debug_write("TSR KEY RECEIVED\r\n");
    } else if (type == IME_CLOSE_IME && ime_active &&
               packet_sequence == open_sequence && payload_length == 0) {
        pc98_debug_write("TSR HOST CLOSE RECEIVED\r\n");
        close_ime();
    }
}

static void accept_byte(unsigned char value)
{
    if (rx_count == 0 && value != IME_MAGIC0)
        return;
    if (rx_count == 1 && value != IME_MAGIC1) {
        rx_count = value == IME_MAGIC0 ? 1 : 0;
        return;
    }
    if (rx_count >= sizeof(rx_packet)) {
        rx_count = 0;
        rx_expected = 0;
        return;
    }
    rx_packet[rx_count++] = value;
    if (rx_count == 8) {
        rx_expected = 10 + (unsigned short)rx_packet[6] +
                      ((unsigned short)rx_packet[7] << 8);
        if (rx_expected > sizeof(rx_packet)) {
            rx_count = 0;
            rx_expected = 0;
        }
    }
    if (rx_expected && rx_count == rx_expected) {
        handle_packet();
        rx_count = 0;
        rx_expected = 0;
    }
}

static void poll_transport(void)
{
    unsigned char budget = 32;
    int value;
    while (budget--) {
        value = pc98_tsr_serial_try_read_byte();
        if (value < 0) {
            value = pc98_serial_try_read_byte();
            if (value >= 0)
                ++direct_poll_reads;
        }
        if (value < 0)
            break;
        accept_byte((unsigned char)value);
    }
}

static void log_transport_diagnostics(void)
{
    pc98_debug_write("TSR DIAG STATUS32 ");
    pc98_debug_write_hex_byte(pc98_serial_status());
    pc98_debug_write("TSR DIAG SYSPORT35 ");
    pc98_debug_write_hex_byte(pc98_serial_system_port());
    pc98_debug_write("TSR DIAG PICMASK02 ");
    pc98_debug_write_hex_byte(pc98_serial_pic_mask());
    pc98_debug_write("TSR DIAG IRQ ENTRY ");
    pc98_debug_write_hex_word(pc98_serial_irq_entry_count);
    pc98_debug_write("TSR DIAG IRQ RXRDY ");
    pc98_debug_write_hex_word(pc98_serial_irq_rx_ready_count);
    pc98_debug_write("TSR DIAG IRQ STORED ");
    pc98_debug_write_hex_word(pc98_serial_irq_stored_count);
    pc98_debug_write("TSR DIAG POLL READ ");
    pc98_debug_write_hex_word(direct_poll_reads);
    pc98_debug_write("TSR DIAG INT18 HOOK ");
    pc98_debug_write_hex_word(pc98_input_hook_count);
    pc98_debug_write("TSR DIAG INT18 AH00 ");
    pc98_debug_write_hex_word(pc98_input_ah00_count);
    pc98_debug_write("TSR DIAG INT18 AH01 ");
    pc98_debug_write_hex_word(pc98_input_ah01_count);
    pc98_debug_write("TSR DIAG INT18 AH02 ");
    pc98_debug_write_hex_word(pc98_input_ah02_count);
    pc98_debug_write("TSR DIAG INT18 AH05 ");
    pc98_debug_write_hex_word(pc98_input_ah05_count);
    pc98_debug_write("TSR DIAG INT18 OTHER ");
    pc98_debug_write_hex_word(pc98_input_other_count);
}

static void inject_pending_text(void)
{
    unsigned char budget = 16;
    unsigned short input;
    unsigned short key;

    while (pending_offset < pending_length && budget--) {
        input = pending_offset;
        key = pc98_bios_encode_key(pending_text, pending_length, &input);
        if (!pc98_bios_enqueue_word(key))
            return;
        pending_offset = input + 1;
    }
    if (pending_offset >= pending_length) {
        send_packet(IME_TEXT_ACK, pending_sequence, 0, 0);
        pending_length = 0;
        pending_offset = 0;
        ime_active = 0;
        open_pending = ime_enabled;
        pc98_debug_write("TSR TEXT QUEUED AND ACKNOWLEDGED\r\n");
    }
}

static void inject_pending_key(void)
{
    if (!pc98_bios_enqueue_word(pending_key))
        return;
    send_packet(IME_KEY_ACK, pending_sequence, 0, 0);
    pending_key_active = 0;
    ime_active = 0;
    open_pending = ime_enabled;
    pc98_debug_write("TSR KEY QUEUED AND ACKNOWLEDGED\r\n");
}

static void start_link(void)
{
    static const unsigned char hello[] = {
        0x01, 0x02, 'C', 'P', '9', '3', '2', 0,
        0x00, 0x02, /* 512-byte resident staged queue */
        0x02        /* injection mode: resident staged BIOS queue */
    };

    direct_poll_reads = 0;
    if (!pc98_serial_open()) {
        return;
    }
    pc98_tsr_serial_capture_begin();
    pc98_debug_write("TSR COM1 IRQ BUFFER\r\n");
    ++sequence;
    if (!send_packet(IME_HELLO, sequence, hello, sizeof(hello))) {
        reset_link();
        return;
    }
    link_state = LINK_HELLO_WAIT;
    wait_ticks = 0;
    diagnostic_delay = 2;
    pc98_debug_write("TSR HELLO SENT\r\n");
}

static void toggle_ime_hotkey(void)
{
    pc98_debug_write("TSR HOTKEY PRESSED\r\n");
    pc98_debug_write("TSR HOTKEY SPACE CONSUMED\r\n");
    if (ime_enabled)
        close_ime();
    else {
        ime_enabled = 1;
        open_pending = 1;
        pc98_debug_write("TSR IME ON\r\n");
    }
}

/* INT 18h AH=00 can remain inside the original BIOS handler while waiting
   for the next key.  If that handler returns the configured modifier+Space, the key is no
   longer in the BIOS queue, so the assembly post-filter calls this entry.
   The returned word is the one-event proof; the raw state is used only to
   distinguish the hotkey from an ordinary Space word. */
int pc98_service_returned_hotkey(void)
{
    if (pc98_poll_hotkey()) {
        toggle_ime_hotkey();
        return 1;
    }
    return 0;
}

void pc98_service_foreground(void)
{
    int hotkey_state = pc98_poll_hotkey();

    /* Toggle only after consuming the matching BIOS word.  This avoids a
       raw-key edge and its later BIOS word being counted as two presses. */
    if (hotkey_state && pc98_bios_discard_hotkey_space())
        toggle_ime_hotkey();

    if (link_state != LINK_CLOSED)
        poll_transport();
    if (pending_length)
        inject_pending_text();
    if (pending_key_active)
        inject_pending_key();

    if (open_pending && !ime_active) {
        if (link_state == LINK_READY)
            send_open_ime();
        else if (link_state == LINK_CLOSED)
            start_link();
    }

    if (link_state == LINK_HELLO_WAIT || ime_active) {
        ++wait_ticks;
        if (diagnostic_delay && --diagnostic_delay == 0)
            log_transport_diagnostics();
        if (link_state == LINK_HELLO_WAIT && wait_ticks == 0x0100) {
            log_transport_diagnostics();
        }
        if (wait_ticks == 0xffff) {
            pc98_debug_write("TSR RESPONSE TIMEOUT\r\n");
            reset_link();
            ime_enabled = 0;
            open_pending = 0;
            pc98_debug_write("TSR IME OFF\r\n");
        }
    }
}
