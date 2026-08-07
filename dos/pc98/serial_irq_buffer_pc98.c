volatile unsigned char pc98_serial_irq_capture;
volatile unsigned char pc98_serial_irq_read_pos;
volatile unsigned char pc98_serial_irq_write_pos;
volatile unsigned char pc98_serial_irq_buffer[128];
volatile unsigned short pc98_serial_irq_entry_count;
volatile unsigned short pc98_serial_irq_rx_ready_count;
volatile unsigned short pc98_serial_irq_stored_count;
volatile unsigned short pc98_input_hook_count;
volatile unsigned short pc98_input_ah00_count;
volatile unsigned short pc98_input_ah01_count;
volatile unsigned short pc98_input_ah02_count;
volatile unsigned short pc98_input_ah05_count;
volatile unsigned short pc98_input_other_count;
static unsigned char saved_pic_mask;
static unsigned char pic_mask_saved;

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

void pc98_tsr_serial_capture_begin(void)
{
    pc98_serial_irq_read_pos = 0;
    pc98_serial_irq_write_pos = 0;
    pc98_serial_irq_entry_count = 0;
    pc98_serial_irq_rx_ready_count = 0;
    pc98_serial_irq_stored_count = 0;
    pc98_serial_irq_capture = 1;
    saved_pic_mask = port_in(0x0002);
    pic_mask_saved = 1;
    port_out(0x0002, (unsigned char)(saved_pic_mask & (unsigned char)~0x10));
}

void pc98_tsr_serial_capture_end(void)
{
    pc98_serial_irq_capture = 0;
    if (pic_mask_saved) {
        port_out(0x0002, saved_pic_mask);
        pic_mask_saved = 0;
    }
    pc98_serial_irq_read_pos = pc98_serial_irq_write_pos;
}

int pc98_tsr_serial_try_read_byte(void)
{
    unsigned char position = pc98_serial_irq_read_pos;
    unsigned char value;

    if (position == pc98_serial_irq_write_pos)
        return -1;
    value = pc98_serial_irq_buffer[position];
    pc98_serial_irq_read_pos = (unsigned char)((position + 1) & 0x7f);
    return value;
}
