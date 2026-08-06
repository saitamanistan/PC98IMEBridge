.code16
.att_syntax prefix

.text
.global device_header

device_header:
    .long 0xffffffff
    # Character device, IOCTL supported, Open/Close requests supported.
    .word 0xc800
    .word strategy_entry
    .word interrupt_entry
    .ascii "IME98$  "

request_offset:
    .word 0
request_segment:
    .word 0
open_count:
    .byte 0
last_command:
    .byte 0xff
session_state:
    .byte 0

strategy_entry:
    movw %bx, %cs:request_offset
    movw %es, %cs:request_segment
    lret

interrupt_entry:
    pushw %ax
    pushw %bx
    pushw %cx
    pushw %dx
    pushw %di
    pushw %ds
    pushw %es
    pushw %cs
    popw %ds
    call load_request
    movb %es:2(%bx), %al
    cmpb $0x00, %al
    je initialize
    cmpb $0x03, %al
    je ioctl_input
    cmpb $0x0c, %al
    je ioctl_output
    cmpb $0x0d, %al
    je device_open
    cmpb $0x0e, %al
    je device_close
    jmp unsupported_command

initialize:
    movw $resident_end, %es:14(%bx)
    pushw %cs
    popw %ax
    movw %ax, %es:16(%bx)
    movw $0x0100, %es:3(%bx)
    movw $load_message, %dx
    movb $0x09, %ah
    int $0x21
    jmp interrupt_done

device_open:
    cmpb $0xff, open_count
    je request_done
    incb open_count
    jmp request_done

device_close:
    cmpb $0x00, open_count
    je request_done
    decb open_count
    jmp request_done

# IOCTL output packet (INT 21h AX=4403h): "I98", version 1, command.
# Commands 0, 1, and 2 are ping, session-open, and session-close.
ioctl_output:
    cmpw $5, %es:18(%bx)
    jb bad_length
    movw %es:14(%bx), %di
    movw %es:16(%bx), %ax
    movw %ax, %es
    cmpb $'I', %es:0(%di)
    jne bad_packet
    cmpb $'9', %es:1(%di)
    jne bad_packet
    cmpb $'8', %es:2(%di)
    jne bad_packet
    cmpb $1, %es:3(%di)
    jne bad_packet
    movb %es:4(%di), %al
    cmpb $2, %al
    ja bad_command
    movb %al, last_command
    cmpb $1, %al
    jne check_session_close
    movb $1, session_state
    jmp command_accepted
check_session_close:
    cmpb $2, %al
    jne command_accepted
    movb $0, session_state
command_accepted:
    call load_request
    movw $5, %es:18(%bx)
    jmp request_done

# IOCTL input packet (INT 21h AX=4402h):
# "I98", version, open count, last command, two reserved bytes.
ioctl_input:
    cmpw $8, %es:18(%bx)
    jb bad_length
    movw %es:14(%bx), %di
    movw %es:16(%bx), %ax
    movw %ax, %es
    movb $'I', %es:0(%di)
    movb $'9', %es:1(%di)
    movb $'8', %es:2(%di)
    movb $1, %es:3(%di)
    movb open_count, %al
    movb %al, %es:4(%di)
    movb last_command, %al
    movb %al, %es:5(%di)
    movb session_state, %al
    movb %al, %es:6(%di)
    movb $0, %es:7(%di)
    call load_request
    movw $8, %es:18(%bx)
    jmp request_done

bad_packet:
    call load_request
    movw $0, %es:18(%bx)
    movw $0x810c, %es:3(%bx)
    jmp interrupt_done

bad_command:
    call load_request
    movw $0, %es:18(%bx)
    movw $0x8103, %es:3(%bx)
    jmp interrupt_done

bad_length:
    movw $0, %es:18(%bx)
    movw $0x8105, %es:3(%bx)
    jmp interrupt_done

unsupported_command:
    movw $0x8103, %es:3(%bx)
    jmp interrupt_done

request_done:
    movw $0x0100, %es:3(%bx)

interrupt_done:
    popw %es
    popw %ds
    popw %di
    popw %dx
    popw %cx
    popw %bx
    popw %ax
    lret

load_request:
    movw request_offset, %bx
    movw request_segment, %ax
    movw %ax, %es
    ret

load_message:
    .ascii "IME98.SYS loaded (safe IOCTL v1)\r\n$"

resident_end:
