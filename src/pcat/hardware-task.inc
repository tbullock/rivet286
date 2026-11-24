; 80286 hardware task entry for a protected small-model client.
%define TASK_HOST 0x6080
%define TASK_USER 0x60b0
%define TASK_PHASE 0x61a0
%define TASK_HOST_SP 0x61a2
%define TASK_RECORD 0x61a4

hardware_launch:
    mov ax, DATA
    mov es, ax
    xor ax, ax
    mov di, TASK_HOST
    mov cx, 22
    rep stosw
    mov di, 0x6140
    mov cx, 43
    rep stosw
    mov bx, gdt_tss
    mov si, TASK_HOST
    call hardware_descriptor
    mov bx, gdt_task
    mov si, TASK_USER
    call hardware_descriptor
    mov word [IDT+0x62*8], 0
    mov word [IDT+0x62*8+2], 0xb0
    mov word [IDT+0x62*8+4], 0x8500
    mov word [IDT+0x62*8+6], 0
    mov ax, 0xa0
    ltr ax
    mov word [TASK_PHASE], 1
    mov word [TASK_RECORD], 0x6140
    call hardware_initialize
    mov [TASK_HOST_SP], sp
    mov bx, 0x1234
    mov cx, 0x2345
    mov dx, 0x3456
    mov si, 0x4567
    mov di, 0x5678
    mov bp, 0x6789
    mov ax, 0x789a
    push word 2
    popf
    call 0xb0:0
hardware_call_return:
    pushf
    pusha
    push ds
    push es
    call hardware_capture
    add sp, 22
    test ax, ax
    jnz hardware_leave
    ; The hardware return must have cleared busy before reinitialization.
    cmp byte [gdt_task+5], 0x81
    jne hardware_bad
    mov word [TASK_PHASE], 2
    mov word [TASK_RECORD], 0x616a
    call hardware_initialize
    mov [TASK_HOST_SP], sp
    mov bx, 0x1234
    mov cx, 0x2345
    mov dx, 0x3456
    mov si, 0x4567
    mov di, 0x5678
    mov bp, 0x6789
    mov ax, 0x789a
    push word 2
    popf
    int 0x62
hardware_interrupt_return:
    pushf
    pusha
    push ds
    push es
    call hardware_capture
    add sp, 22
    test ax, ax
    jnz hardware_leave
    xor ax, ax
hardware_leave:
    clts
    jmp program_return
hardware_bad:
    mov ax, 75
    jmp hardware_leave

; DS=ES=DATA; install an available 44-byte 286 TSS descriptor.
hardware_descriptor:
    mov ax, [dos_cs]
    mov dx, ax
    shl ax, 4
    shr dx, 12
    add ax, si
    adc dx, 0
    mov word [bx], 0x2b
    mov [bx+2], ax
    mov [bx+4], dl
    mov byte [bx+5], 0x81
    mov word [bx+6], 0
    ret

hardware_initialize:
    xor ax, ax
    mov di, TASK_USER
    mov cx, 22
    rep stosw
    mov ax, [MB+42]
    mov [TASK_USER+0x0e], ax
    mov word [TASK_USER+0x10], 2
    mov word [TASK_USER+0x12], 0x0100
    mov ax, [MB+4]
    xor ax, [TASK_PHASE]
    mov [TASK_USER+0x16], ax
    mov ax, [TASK_PHASE]
    mov [TASK_USER+0x18], ax
    mov ax, [MB+44]
    mov [TASK_USER+0x1a], ax
    mov word [TASK_USER+0x22], 0x40
    mov word [TASK_USER+0x24], 0x38
    mov word [TASK_USER+0x26], 0x40
    mov word [TASK_USER+0x28], 0x40
    ret

; Capture first; caller's full resumed image is on SS:SP. Twenty-one words/phase:
; phase, hostTR, hostMSW, hostFLAGS, hostSP, hostDS, hostES,
; target backlink/access/IP/FLAGS/AX/SP/ES/CS/SS/DS, hostSS/access, VALUE, TESTS.
hardware_capture:
    mov bp, sp
    mov di, [cs:TASK_RECORD]
    mov ax, DATA
    mov ds, ax
    mov es, ax
    cld
    mov ax, [TASK_PHASE]
    stosw
    str ax
    stosw
    smsw ax
    stosw
    mov ax, [ss:bp+22]
    stosw
    lea ax, [bp+24]
    stosw
    mov ax, [ss:bp+4]
    stosw
    mov ax, [ss:bp+2]
    stosw
    mov ax, [TASK_USER]
    stosw
    xor ax, ax
    mov al, [gdt_task+5]
    stosw
    mov ax, [TASK_USER+0x0e]
    stosw
    mov ax, [TASK_USER+0x10]
    stosw
    mov ax, [TASK_USER+0x12]
    stosw
    mov ax, [TASK_USER+0x1a]
    stosw
    mov si, TASK_USER+0x22
    mov cx, 4
    rep movsw
    mov ax, ss
    stosw
    xor ax, ax
    mov al, [gdt_tss+5]
    stosw
    mov ax, [MB+34]
    stosw
    mov ax, [MB+36]
    stosw
    cmp word [ss:bp+20], 0x789a
    jne .bad
    cmp word [ss:bp+22], 2
    jne .bad
    cmp word [ss:bp+4], DATA
    jne .bad
    cmp word [ss:bp+2], DATA
    jne .bad
    mov ax, ss
    cmp ax, DATA
    jne .bad
    ; Original host general registers are checked before any next switch.
    cmp word [ss:bp+6], 0x5678
    jne .bad
    cmp word [ss:bp+8], 0x4567
    jne .bad
    cmp word [ss:bp+10], 0x6789
    jne .bad
    cmp word [ss:bp+14], 0x1234
    jne .bad
    cmp word [ss:bp+16], 0x3456
    jne .bad
    cmp word [ss:bp+18], 0x2345
    jne .bad
    lea ax, [bp+24]
    cmp ax, [TASK_HOST_SP]
    jne .bad
    str ax
    cmp ax, 0xa0
    jne .bad
    cmp byte [gdt_tss+5], 0x83
    jne .bad
    cmp byte [gdt_task+5], 0x81
    jne .bad
    cmp word [TASK_USER], 0xa0
    jne .bad
    test word [TASK_USER+0x10], 0x4000
    jnz .bad
    cmp word [TASK_USER+0x12], 0
    jne .bad
    cmp word [MB+36], 0xffff
    jne .bad
    xor ax, ax
    ret
.bad:
    mov ax, 75
    ret

; Compact fixed labels HAA..HAU, HBA..HBU for the two task records.
hardware_print:
    mov di, 0x6140
    mov bp, 2
    mov byte [hardware_name+1], 'A'
.phase:
    mov bx, 21
    mov byte [hardware_name+2], 'A'
.field:
    mov si, hardware_name
    call print
    mov ax, [di]
    call hex
    mov si, newline
    call print
    add di, 2
    inc byte [hardware_name+2]
    dec bx
    jnz .field
    inc byte [hardware_name+1]
    dec bp
    jnz .phase
    ret
hardware_name: db 'HAA=',0
