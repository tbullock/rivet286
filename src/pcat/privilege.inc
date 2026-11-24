; Separately admitted ABI0.5 small fixture; no DOS reset while CPL3 is live.
%define PRIV_TSS 0x6080
%define PRIV_ARMED 0x6140
%define PRIV_IP 0x6142
%define PRIV_RECOVER 0x6144
%define PRIV_ERROR 0x6146
%define PRIV_SP 0x6148
%define PRIV_COUNT 0x614a
%define PRIV_RECORDS 0x6150

privilege_launch:
    mov ax, DATA
    mov ds, ax
    mov es, ax
    xor ax, ax
    mov di, PRIV_TSS
    mov cx, 22
    rep stosw
    mov di, PRIV_ARMED
    mov cx, 88
    rep stosw
    mov word [PRIV_TSS+2], 0xd000
    mov word [PRIV_TSS+4], DATA
    mov ax, [dos_cs]
    mov dx, ax
    shl ax, 4
    shr dx, 12
    add ax, PRIV_TSS
    adc dx, 0
    mov [gdt_tss+2], ax
    mov [gdt_tss+4], dl
    mov word [gdt_tss], 0x2b
    mov byte [gdt_tss+5], 0x81
    mov byte [gdt_client_code+5], 0xfa
    mov byte [gdt_client_data+5], 0xf2
    mov word [IDT+0x61*8], privilege_service
    mov word [IDT+0x61*8+4], 0xe600
    mov word [gdt_gate], privilege_gate_entry
    mov word [gdt_gate+2], CODE
    mov word [gdt_gate+4], 0xe401
    mov word [gdt_gate+6], 0
    mov ax, 0xa0
    ltr ax
    push word 0x43
    push word [MB+44]
    push word 2
    push word 0x3b
    push word [MB+42]
    mov ax, 0x43
    mov ds, ax
    mov es, ax
    iret

privilege_service:
    push bp
    mov bp, sp
    pusha
    push ds
    push es
    mov ax, DATA
    mov ds, ax
    mov es, ax
    cld
    cmp word [PRIV_FLAG], 1
    jne panic
    cmp word [ss:bp+4], 0x3b
    jne panic
    cmp word [ss:bp+10], 0x43
    jne panic
    test word [ss:bp+6], 0x7300
    jnz panic
    inc word [MB+24]
    mov ax, [ss:bp-2]
    test ax, ax
    jz .version
    cmp ax, 1
    je .report
    cmp ax, 2
    je .seed
    cmp ax, 3
    je .arm
    cmp ax, 4
    je .callback
    cmp ax, 5
    je .exit
    cmp ax, 6
    je .count
.bad:
    mov ax, 1
    or word [ss:bp+6], 1
    jmp .return
.version:
    mov ax, 0x0100
    jmp .ok
.report:
    mov ax, [ss:bp-8]
    mov [MB+34], ax
    mov ax, [ss:bp-4]
    mov [MB+36], ax
    xor ax, ax
    jmp .ok
.seed:
    mov ax, [MB+4]
    jmp .ok
.arm:
    cmp word [PRIV_ARMED], 0
    jne .bad
    cmp word [PRIV_COUNT], 5
    jae .bad
    mov ax, [ss:bp-6]
    test ax, ax
    jz .error_ok
    cmp ax, 0x10
    je .error_ok
    cmp ax, 0x302
    jne .bad
.error_ok:
    mov [PRIV_ERROR], ax
    mov ax, 0x3b
    lsl dx, ax
    jnz .bad
    mov ax, [ss:bp-8]
    cmp ax, dx
    ja .bad
    mov [PRIV_IP], ax
    mov ax, [ss:bp-4]
    cmp ax, dx
    ja .bad
    cmp ax, [PRIV_IP]
    je .bad
    mov [PRIV_RECOVER], ax
    mov ax, [ss:bp+8]
    mov [PRIV_SP], ax
    mov word [PRIV_ARMED], 1
    xor ax, ax
    jmp .ok
.callback:
    cmp word [MB+52], 0
    je .bad
    push word [ss:bp-8]
    call word [MB+52]
    add sp, 2
    jmp .ok
.count:
    mov ax, [PRIV_COUNT]
    jmp .ok
.exit:
    mov ax, [ss:bp-8]
    test ax, ax
    jnz .leave
    cmp word [PRIV_ARMED], 0
    jne .incomplete
    cmp word [PRIV_COUNT], 5
    jne .incomplete
    cmp word [0x614c], 5
    je .leave
.incomplete:
    mov ax, 74
.leave:
    jmp program_return
.ok:
    and word [ss:bp+6], 0xfffe
.return:
    mov [ss:bp-2], ax
    pop es
    pop ds
    popa
    pop bp
    iret

; Entered from the existing normalized error/no-error exception adapter.
privilege_fault:
    cmp word [ss:bp+8], 0x3b
    jne panic
    cmp word [ss:bp+14], 0x43
    jne panic
    test word [ss:bp+10], 0x7300
    jnz panic
    cmp word [PRIV_ARMED], 1
    jne panic
    cmp word [ss:bp+2], 13
    jne panic
    mov ax, [PRIV_ERROR]
    cmp [ss:bp+4], ax
    jne panic
    mov ax, [PRIV_IP]
    cmp [ss:bp+6], ax
    jne panic
    mov ax, [PRIV_SP]
    cmp [ss:bp+12], ax
    jne panic
    cmp ax, [MB+44]
    ja panic
    mov dx, [HEADER+22]
    sub dx, [HEADER+20]
    cmp ax, dx
    jb panic
    mov bx, [PRIV_COUNT]
    cmp bx, 5
    jae panic
    shl bx, 4
    add bx, PRIV_RECORDS
    mov si, bp
    add si, 2
    mov cx, 7
.copy:
    mov ax, [ss:si]
    mov [bx], ax
    add si, 2
    add bx, 2
    loop .copy
    mov ax, [PRIV_RECOVER]
    mov [bx], ax
    mov [ss:bp+6], ax
    mov word [PRIV_ARMED], 0
    inc word [PRIV_COUNT]
    pop es
    pop ds
    popa
    pop bp
    add sp, 4
    iret

privilege_print:
    mov si, priv_count_name
    call print
    mov ax, [PRIV_COUNT]
    call hex
    mov si, newline
    call print
    mov si, priv_names
    mov bx, PRIV_RECORDS
    mov cx, 40
.fields:
    push cx
    push si
    mov si, [si]
    call print
    mov ax, [bx]
    call hex
    mov si, newline
    call print
    pop si
    add si, 2
    add bx, 2
    pop cx
    loop .fields
    ret
priv_count_name: db 'PCOUNT=',0
priv_names: dw priv_name_0,priv_name_1,priv_name_2,priv_name_3,priv_name_4,priv_name_5,priv_name_6,priv_name_7,priv_name_8,priv_name_9,priv_name_10,priv_name_11,priv_name_12,priv_name_13,priv_name_14,priv_name_15,priv_name_16,priv_name_17,priv_name_18,priv_name_19,priv_name_20,priv_name_21,priv_name_22,priv_name_23,priv_name_24,priv_name_25,priv_name_26,priv_name_27,priv_name_28,priv_name_29,priv_name_30,priv_name_31,priv_name_32,priv_name_33,priv_name_34,priv_name_35,priv_name_36,priv_name_37,priv_name_38,priv_name_39
priv_name_0: db 'PVA=',0
priv_name_1: db 'PEA=',0
priv_name_2: db 'PIA=',0
priv_name_3: db 'PCA=',0
priv_name_4: db 'PFA=',0
priv_name_5: db 'PSA=',0
priv_name_6: db 'PDA=',0
priv_name_7: db 'PRA=',0
priv_name_8: db 'PVB=',0
priv_name_9: db 'PEB=',0
priv_name_10: db 'PIB=',0
priv_name_11: db 'PCB=',0
priv_name_12: db 'PFB=',0
priv_name_13: db 'PSB=',0
priv_name_14: db 'PDB=',0
priv_name_15: db 'PRB=',0
priv_name_16: db 'PVC=',0
priv_name_17: db 'PEC=',0
priv_name_18: db 'PIC=',0
priv_name_19: db 'PCC=',0
priv_name_20: db 'PFC=',0
priv_name_21: db 'PSC=',0
priv_name_22: db 'PDC=',0
priv_name_23: db 'PRC=',0
priv_name_24: db 'PVD=',0
priv_name_25: db 'PED=',0
priv_name_26: db 'PID=',0
priv_name_27: db 'PCD=',0
priv_name_28: db 'PFD=',0
priv_name_29: db 'PSD=',0
priv_name_30: db 'PDD=',0
priv_name_31: db 'PRD=',0
priv_name_32: db 'PVE=',0
priv_name_33: db 'PEE=',0
priv_name_34: db 'PIE=',0
priv_name_35: db 'PCE=',0
priv_name_36: db 'PFE=',0
priv_name_37: db 'PSE=',0
priv_name_38: db 'PDE=',0
priv_name_39: db 'PRE=',0
