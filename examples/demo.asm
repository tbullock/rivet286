; PM16 small-model program that creates a file through Rivet's DOS gateway.
bits 16
cpu 286

DATA_SELECTOR equ 0x0040
DATA_OFFSET   equ 0x0100
STACK_TOP     equ 0xfffe
STACK_BYTES   equ 512

    db 'PM16'
    dw 0x0100, 32, 1
    dw code_end - code, 0
    dw DATA_OFFSET, data_end - data, 0
    dw STACK_BYTES, STACK_TOP
    dd data_end - $$
    dd 0

code:
    mov bx, DATA_OFFSET + create_request - data
    call dos_request
    jc finish
    mov dx, [bx + 24]              ; Rivet's handle, separate from the DOS handle
    mov [DATA_OFFSET + write_request - data + 6], dx
    mov [DATA_OFFSET + close_request - data + 6], dx

    mov bx, DATA_OFFSET + write_request - data
    call dos_request
    jc finish
    cmp word [bx + 24], message_end - message
    jne short_write

    mov bx, DATA_OFFSET + close_request - data
    call dos_request
finish:
    retf

short_write:
    mov ax, 1
    retf

dos_request:
    mov ax, 0x0100
    mov cx, 32
    int 0x60
    ret

code_end:
data:
create_request:
    dw 1, 3, 0, 0, 1, 0
    dw DATA_OFFSET + filename - data, DATA_SELECTOR
    dd filename_end - filename, 0, 0
    dw 0, 0

write_request:
    dw 1, 5, 0, 0, 1, 0
    dw DATA_OFFSET + message - data, DATA_SELECTOR
    dd message_end - message, 0, 0
    dw 0, 0

close_request:
    dw 1, 7
    times 28 db 0

filename:     db 'RIVET.TXT', 0
filename_end:
message:      db 'Hello from Rivet/286.', 13, 10
message_end:
data_end:
