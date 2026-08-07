.code16
.att_syntax prefix

.text
.global pc98_input_hook
.global pc98_install_input
.global pc98_restore_input
.global pc98_poll_hotkey
.global pc98_serial_irq_hook
.global pc98_install_serial_irq
.global pc98_restore_serial_irq
.global pc98_uninstall_resident
.global pc98_uninstall_prepare_entry
.global pc98_worker_stack_top
.extern pc98_service_foreground
.extern pc98_service_returned_hotkey
.extern pc98_prepare_uninstall
.extern pc98_hotkey_latched
.extern pc98_hotkey_modifier_mask
.extern pc98_serial_data_port
.extern pc98_serial_status_port
.extern pc98_serial_irq_capture
.extern pc98_serial_irq_read_pos
.extern pc98_serial_irq_write_pos
.extern pc98_serial_irq_buffer
.extern pc98_serial_irq_entry_count
.extern pc98_serial_irq_rx_ready_count
.extern pc98_serial_irq_stored_count
.extern pc98_input_hook_count
.extern pc98_input_ah00_count
.extern pc98_input_ah01_count
.extern pc98_input_ah02_count
.extern pc98_input_ah05_count
.extern pc98_input_other_count

pc98_input_hook:
    pushf
    movb %ah, %cs:pc98_input_original_ah
    incw %cs:pc98_input_hook_count
    cmpb $0x00, %ah
    je pc98_input_count_ah00
    cmpb $0x01, %ah
    je pc98_input_count_ah01
    cmpb $0x02, %ah
    je pc98_input_count_ah02
    cmpb $0x05, %ah
    je pc98_input_count_ah05
    incw %cs:pc98_input_other_count
    jmp pc98_input_count_done
pc98_input_count_ah00:
    incw %cs:pc98_input_ah00_count
    jmp pc98_input_count_done
pc98_input_count_ah01:
    incw %cs:pc98_input_ah01_count
    jmp pc98_input_count_done
pc98_input_count_ah02:
    incw %cs:pc98_input_ah02_count
    jmp pc98_input_count_done
pc98_input_count_ah05:
    incw %cs:pc98_input_ah05_count
pc98_input_count_done:
    pushw %ax
    pushw %bx
    pushw %cx
    pushw %dx
    pushw %si
    pushw %di
    pushw %bp
    pushw %ds
    pushw %es
    pushw %cs
    popw %ds

    /* Interrupt callers own an arbitrary SS.  The C worker passes pointers to
       stack locals, so DS and SS must address the same resident segment. */
    cli
    movw %ss, %ax
    movw %ax, %cs:pc98_worker_old_ss
    movw %sp, %cs:pc98_worker_old_sp
    pushw %cs
    popw %ss
    movw $pc98_worker_stack_top, %sp
    call pc98_service_foreground

    cli
    movw %cs:pc98_worker_old_ss, %ax
    movw %cs:pc98_worker_old_sp, %bx
    movw %ax, %ss
    movw %bx, %sp
    popw %es
    popw %ds
    popw %bp
    popw %di
    popw %si
    popw %dx
    popw %cx
    popw %bx
    popw %ax
    popf

pc98_input_call_old:
    pushf
    lcall *%cs:pc98_old_input_vector
    cmpw $0x3420, %ax
    jne pc98_input_return
pc98_input_returned_space:
    /* A blocking BIOS read removes the word before returning it, so the
       normal queue consumer cannot see this modifier+Space hotkey.  Run the dedicated
       post-return handler on the resident stack before suppressing it. */
    pushf
    pushw %ax
    pushw %bx
    pushw %cx
    pushw %dx
    pushw %si
    pushw %di
    pushw %bp
    pushw %ds
    pushw %es
    pushw %cs
    popw %ds
    cli
    movw %ss, %ax
    movw %ax, %cs:pc98_worker_old_ss
    movw %sp, %cs:pc98_worker_old_sp
    pushw %cs
    popw %ss
    movw $pc98_worker_stack_top, %sp
    call pc98_service_returned_hotkey
    movb %al, %cs:pc98_returned_hotkey_handled
    cli
    movw %cs:pc98_worker_old_ss, %ax
    movw %cs:pc98_worker_old_sp, %bx
    movw %ax, %ss
    movw %bx, %sp
    popw %es
    popw %ds
    popw %bp
    popw %di
    popw %si
    popw %dx
    popw %cx
    popw %bx
    popw %ax
    popf
    cmpb $0, %cs:pc98_returned_hotkey_handled
    je pc98_input_return
    cmpb $0x00, %cs:pc98_input_original_ah
    je pc98_input_retry_blocking
    cmpb $0x05, %cs:pc98_input_original_ah
    jne pc98_input_return
    xorb %bh, %bh
    jmp pc98_input_return

pc98_input_retry_blocking:
    /* Do not re-enter the BIOS AH=00 wait after suppressing the hotkey.
       np21w's emulated blocking path does not reliably notice a queue word
       inserted by our COM1 interrupt worker.  Wait on the caller's stack,
       then remove the next BIOS word ourselves.  Both the keyboard IRQ and
       COM1 IRQ wake HLT, so injected text is returned immediately. */
pc98_input_wait_queue:
    cli
    pushw %bx
    pushw %es
    xorw %ax, %ax
    movw %ax, %es
    cmpb $0, %es:0x0528
    jne pc98_input_dequeue
    popw %es
    popw %bx
    sti
    hlt
    jmp pc98_input_wait_queue

pc98_input_dequeue:
    decb %es:0x0528
    movw %es:0x0524, %bx
    movw %es:(%bx), %ax
    addw $2, %bx
    cmpw $0x0522, %bx
    jb pc98_input_store_head
    movw $0x0502, %bx
pc98_input_store_head:
    movw %bx, %es:0x0524
    popw %es
    popw %bx
    cmpw $0x3420, %ax
    je pc98_input_returned_space
    jmp pc98_input_return

pc98_input_return:
    iret

/* np21w rev103's BIOS keeps the raw key-down bitmap at physical 0000:052a
   and modifier state at 0000:053a.  The configured mask selects Shift
   (bit 0), Graph (bit 3), or Ctrl (bit 4).  Space (34h) maps to bitmap byte
   06h, bit 4.  Polling these bytes does not consume port 41h or the BIOS
   queue. */
pc98_poll_hotkey:
    pushw %es
    xorw %ax, %ax
    movw %ax, %es
    movb %cs:pc98_hotkey_modifier_mask, %al
    testb %al, %es:0x053a
    jz pc98_hotkey_released
    testb $0x10, %es:0x0530
    jz pc98_hotkey_released
    cmpb $0, %cs:pc98_hotkey_latched
    jne pc98_hotkey_held
    movb $1, %cs:pc98_hotkey_latched
    movw $1, %ax
    popw %es
    ret

pc98_hotkey_held:
    movw $2, %ax
    popw %es
    ret

pc98_hotkey_released:
    movb $0, %cs:pc98_hotkey_latched
pc98_hotkey_no_edge:
    xorw %ax, %ax
    popw %es
    ret

/* PC-98 COM1 receive interrupt (INT 0Ch).  Capture only while the TSR owns
   an active protocol transaction.  No C code or DOS service runs here. */
pc98_serial_irq_hook:
    cmpb $0, %cs:pc98_serial_irq_capture
    je pc98_serial_irq_chain
    pushw %ax
    pushw %bx
    pushw %dx
    incw %cs:pc98_serial_irq_entry_count
    movw %cs:pc98_serial_status_port, %dx
    inb %dx, %al
    testb $0x02, %al
    jz pc98_serial_irq_not_receive
    incw %cs:pc98_serial_irq_rx_ready_count
    movw %cs:pc98_serial_data_port, %dx
    inb %dx, %al
    movb %cs:pc98_serial_irq_write_pos, %bl
    xorb %bh, %bh
    movb %bl, %ah
    incb %ah
    andb $0x7f, %ah
    cmpb %cs:pc98_serial_irq_read_pos, %ah
    je pc98_serial_irq_eoi
    movb %al, %cs:pc98_serial_irq_buffer(%bx)
    movb %ah, %cs:pc98_serial_irq_write_pos
    incw %cs:pc98_serial_irq_stored_count
pc98_serial_irq_eoi:
    movw $0x0000, %dx
    movb $0x20, %al
    outb %al, %dx
    pushw %cx
    pushw %si
    pushw %di
    pushw %bp
    pushw %ds
    pushw %es
    pushw %cs
    popw %ds

    /* The np21w BIOS-emulation timer path bypasses guest INT 08h/1Ch.
       After acknowledging a real COM1 receive IRQ, run the bounded worker
       on its resident stack before returning to the waiting BIOS input. */
    cli
    movw %ss, %ax
    movw %ax, %cs:pc98_worker_old_ss
    movw %sp, %cs:pc98_worker_old_sp
    pushw %cs
    popw %ss
    movw $pc98_worker_stack_top, %sp
    call pc98_service_foreground

    cli
    movw %cs:pc98_worker_old_ss, %ax
    movw %cs:pc98_worker_old_sp, %bx
    movw %ax, %ss
    movw %bx, %sp
    popw %es
    popw %ds
    popw %bp
    popw %di
    popw %si
    popw %cx
    popw %dx
    popw %bx
    popw %ax
    iret
pc98_serial_irq_not_receive:
    popw %dx
    popw %bx
    popw %ax
pc98_serial_irq_chain:
    ljmp *%cs:pc98_old_serial_irq_vector

/* Called with a far call from the transient /U copy.  Run the shutdown code
   with the resident DS, SS, and private stack so no transient state is used
   while COM1 capture and the saved PIC mask are being restored. */
pc98_uninstall_prepare_entry:
    pushf
    cli
    pushw %ax
    pushw %bx
    pushw %cx
    pushw %dx
    pushw %si
    pushw %di
    pushw %bp
    pushw %ds
    pushw %es
    movw %ss, %ax
    movw %ax, %cs:pc98_worker_old_ss
    movw %sp, %cs:pc98_worker_old_sp
    pushw %cs
    popw %ds
    pushw %cs
    popw %ss
    movw $pc98_worker_stack_top, %sp
    cld
    call pc98_prepare_uninstall
    cli
    movw %cs:pc98_worker_old_ss, %ax
    movw %cs:pc98_worker_old_sp, %bx
    movw %ax, %ss
    movw %bx, %sp
    popw %es
    popw %ds
    popw %bp
    popw %di
    popw %si
    popw %dx
    popw %cx
    popw %bx
    popw %ax
    popf
    lret

pc98_install_input:
    movw $0x3518, %ax
    int $0x21
    movw $pc98_input_hook, %ax
    cmpw %ax, %bx
    jne pc98_install_input_new
    movw $1, %ax
    ret

pc98_install_input_new:
    movw %bx, pc98_old_input_vector
    movw %es, pc98_old_input_vector+2
    movw $0x2518, %ax
    movw $pc98_input_hook, %dx
    int $0x21
    xorw %ax, %ax
    ret

pc98_restore_input:
    movw $0x3518, %ax
    int $0x21
    pushw %cs
    popw %dx
    pushw %es
    popw %ax
    cmpw %dx, %ax
    jne pc98_restore_input_not_owner
    movw $pc98_input_hook, %ax
    cmpw %ax, %bx
    jne pc98_restore_input_not_owner
    movw $0x2518, %ax
    movw $pc98_old_input_vector, %dx
    int $0x21
    xorw %ax, %ax
    ret

pc98_restore_input_not_owner:
    movw $1, %ax
    ret

pc98_install_serial_irq:
    movw $0x350c, %ax
    int $0x21
    movw $pc98_serial_irq_hook, %ax
    cmpw %ax, %bx
    jne pc98_install_serial_irq_new
    movw $1, %ax
    ret

pc98_install_serial_irq_new:
    movw %bx, pc98_old_serial_irq_vector
    movw %es, pc98_old_serial_irq_vector+2
    movw $0x250c, %ax
    movw $pc98_serial_irq_hook, %dx
    int $0x21
    xorw %ax, %ax
    ret

pc98_restore_serial_irq:
    movw $0x350c, %ax
    int $0x21
    pushw %cs
    popw %dx
    pushw %es
    popw %ax
    cmpw %dx, %ax
    jne pc98_restore_serial_irq_not_owner
    movw $pc98_serial_irq_hook, %ax
    cmpw %ax, %bx
    jne pc98_restore_serial_irq_not_owner
    movw $0x250c, %ax
    movw $pc98_old_serial_irq_vector, %dx
    int $0x21
    xorw %ax, %ax
    ret

pc98_restore_serial_irq_not_owner:
    movw $1, %ax
    ret

/* Locate the resident copy through its owned INT 18h vector, verify that it
   also directly owns INT 0Ch, stop its COM/PIC state, restore both previous
   vectors, then release its environment and PSP memory blocks.  Return 0 on
   complete removal, 1 when ownership/signature checks fail, 2 when only the
   environment block cannot be released, or 3 when the resident block cannot
   be released. */
pc98_uninstall_resident:
    pushw %bp
    pushw %si
    pushw %di
    pushw %ds
    pushw %es

    movw $0x3518, %ax
    int $0x21
    cmpw $pc98_input_hook, %bx
    jne pc98_uninstall_not_owner
    cmpw $0x4953, %es:pc98_tsr_signature
    jne pc98_uninstall_not_owner
    cmpw $0x464f, %es:pc98_tsr_signature+2
    jne pc98_uninstall_not_owner
    cmpw $0x5045, %es:pc98_tsr_signature+4
    jne pc98_uninstall_not_owner
    cmpw $0x3155, %es:pc98_tsr_signature+6
    jne pc98_uninstall_not_owner
    movw %es, %ax
    movw %ax, pc98_uninstall_resident_segment

    movw $0x350c, %ax
    int $0x21
    cmpw $pc98_serial_irq_hook, %bx
    jne pc98_uninstall_not_owner
    movw %es, %ax
    cmpw pc98_uninstall_resident_segment, %ax
    jne pc98_uninstall_not_owner

    movw pc98_uninstall_resident_segment, %ax
    movw %ax, %es
    movw %es:0x002c, %ax
    movw %ax, pc98_uninstall_environment_segment
    movw $pc98_uninstall_prepare_entry, pc98_uninstall_prepare_pointer
    movw pc98_uninstall_resident_segment, %ax
    movw %ax, pc98_uninstall_prepare_pointer+2
    lcall *pc98_uninstall_prepare_pointer

    movw pc98_uninstall_resident_segment, %ax
    movw %ax, %es
    movw %es:pc98_old_serial_irq_vector, %dx
    movw %es:pc98_old_serial_irq_vector+2, %ax
    pushw %ds
    movw %ax, %ds
    movw $0x250c, %ax
    int $0x21
    popw %ds

    movw pc98_uninstall_resident_segment, %ax
    movw %ax, %es
    movw %es:pc98_old_input_vector, %dx
    movw %es:pc98_old_input_vector+2, %ax
    pushw %ds
    movw %ax, %ds
    movw $0x2518, %ax
    int $0x21
    popw %ds

    movw $0, pc98_uninstall_result
    movw pc98_uninstall_environment_segment, %ax
    testw %ax, %ax
    jz pc98_uninstall_free_resident
    movw %ax, %es
    movw $0x4900, %ax
    int $0x21
    jnc pc98_uninstall_free_resident
    movw $2, pc98_uninstall_result

pc98_uninstall_free_resident:
    movw pc98_uninstall_resident_segment, %ax
    movw %ax, %es
    movw $0x4900, %ax
    int $0x21
    jnc pc98_uninstall_done
    movw $3, pc98_uninstall_result

pc98_uninstall_done:
    movw pc98_uninstall_result, %ax
    jmp pc98_uninstall_return

pc98_uninstall_not_owner:
    movw $1, %ax

pc98_uninstall_return:
    popw %es
    popw %ds
    popw %di
    popw %si
    popw %bp
    ret

.bss
.align 2
pc98_worker_old_ss:
    .word 0
pc98_worker_old_sp:
    .word 0
pc98_input_original_ah:
    .byte 0
pc98_returned_hotkey_handled:
    .byte 0
.align 2
pc98_worker_stack:
    .space 1024
pc98_worker_stack_top:

.align 2
pc98_uninstall_resident_segment:
    .word 0
pc98_uninstall_environment_segment:
    .word 0
pc98_uninstall_prepare_pointer:
    .word 0, 0
pc98_uninstall_result:
    .word 0

.section .rodata
.global pc98_tsr_signature
pc98_tsr_signature:
    .ascii "P98IMBU1"
