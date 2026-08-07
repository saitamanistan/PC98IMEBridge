#include <stdint.h>
#include "debug_serial_pc98.h"

unsigned short pc98_old_idle_vector[2];
unsigned short pc98_old_input_vector[2];
unsigned short pc98_old_serial_irq_vector[2];
/* Reserved vector storage enforced by tools/check_pc98_tsr_map.py. The INT
   08h timer and INT 28h idle hooks are intentionally not installed; these
   words stay inside the resident boundary for future/backup use. */
unsigned short pc98_old_timer_vector[2];
extern unsigned char pc98_tsr_resident_end;
extern unsigned char pc98_tsr_bss_start;

int pc98_install_input(void);
int pc98_restore_input(void);
int pc98_install_serial_irq(void);
int pc98_restore_serial_irq(void);
int pc98_uninstall_resident(void);
int pc98_serial_load_config(void);
void pc98_hotkey_set_defaults(void);
extern volatile unsigned char pc98_hotkey_modifier_mask;

static void dos_print(const char *text)
{
    while (*text) {
        uint16_t ax = 0x0200 | (unsigned char)*text++;
        uint16_t dx = (unsigned char)text[-1];
        __asm__ volatile ("int $0x21" : "+a" (ax) : "d" (dx) : "cc");
    }
}

static void dos_exit(unsigned char code)
{
    uint16_t ax = 0x4C00 | code;
    __asm__ volatile ("int $0x21" : : "a" (ax) : "cc");
    for (;;)
        ;
}

static void print_hotkey_binding(void)
{
    const char *hotkey = "Shift+Space";

    if (pc98_hotkey_modifier_mask == 0x08)
        hotkey = "Graph+Space";
    else if (pc98_hotkey_modifier_mask == 0x10)
        hotkey = "Ctrl+Space";

    dos_print("IME98 TSR resident (");
    dos_print(hotkey);
    dos_print(": IME ON / Bridge Esc: IME OFF)\r\n");
}

static int uninstall_requested(int argc, char **argv)
{
    const char *option;

    if (argc < 2)
        return 0;
    option = argv[1];
    return (option[0] == '/' || option[0] == '-') &&
           (option[1] == 'U' || option[1] == 'u') && option[2] == 0;
}

static void stay_resident(void)
{
    uint16_t paragraphs = ((uint16_t)(uintptr_t)&pc98_tsr_resident_end + 16) >> 4;
    uint16_t ax = 0x3100;
    uint16_t dx = paragraphs;
    __asm__ volatile ("int $0x21" : : "a" (ax), "d" (dx) : "cc");
    for (;;)
        ;
}

static void clear_resident_state(void)
{
    unsigned char *position = &pc98_tsr_bss_start;
    while (position < &pc98_tsr_resident_end)
        *position++ = 0;
}

int main(int argc, char **argv)
{
    int input_result;
    int serial_irq_result;

    /* DOS .COM loading does not guarantee that non-file-backed BSS contains
       zeroes.  Stale pending lengths can otherwise be mistaken for text. */
    clear_resident_state();
    pc98_hotkey_set_defaults();

    if (uninstall_requested(argc, argv)) {
        int uninstall_result = pc98_uninstall_resident();
        if (uninstall_result == 0) {
            dos_print("IME98 TSR unloaded; memory released\r\n");
            dos_exit(0);
        }
        if (uninstall_result == 2) {
            dos_print("IME98 TSR unloaded; environment memory not released\r\n");
            dos_exit(2);
        }
        if (uninstall_result == 3) {
            dos_print("IME98 TSR hooks removed; resident memory not released\r\n");
            dos_exit(3);
        }
        dos_print("IME98 TSR is not installed or cannot be unloaded safely\r\n");
        dos_exit(1);
    }

    dos_print("IME98 TSR installing\r\n");
    pc98_debug_open();
    pc98_serial_load_config();
    serial_irq_result = pc98_install_serial_irq();
    if (serial_irq_result) {
        dos_print("IME98 TSR serial hook already resident\r\n");
        dos_exit(1);
    }
    input_result = pc98_install_input();
    if (input_result) {
        pc98_restore_serial_irq();
        dos_print("IME98 TSR input hook already resident\r\n");
        dos_exit(1);
    }
    print_hotkey_binding();
    stay_resident();
    return 0;
}
