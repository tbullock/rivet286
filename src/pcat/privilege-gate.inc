; 286 interprivilege call gate with one hardware-copied word parameter.
%define PRIV_GATE_COUNT 0x614c
%define PRIV_GATE_RECORDS 0x61a0
privilege_gate_entry:
    push bp
    mov bp, sp
    pushf
    pusha
    push ds
    push es
    mov ax, DATA
    mov ds, ax
    mov es, ax
    cld
    cmp word [PRIV_FLAG], 1
    jne panic
    ; Same-ring calls have no outer SS:SP. Reject them before reading those.
    cmp word [ss:bp+4], 0x3b
    jne panic
    cmp word [ss:bp+10], 0x43
    jne panic
    test word [ss:bp-2], 0x7300
    jnz panic
    mov ax, ss
    cmp ax, DATA
    jne panic
    mov ax, [ss:bp+8]
    test ax, 1
    jnz panic
    mov dx, [HEADER+22]
    sub dx, [HEADER+20]
    cmp ax, dx
    jb panic
    add ax, 2
    jc panic
    cmp ax, [MB+44]
    ja panic
    mov bx, [PRIV_GATE_COUNT]
    cmp bx, 5
    jae panic
    shl bx, 4
    add bx, PRIV_GATE_RECORDS
    mov ax, [ss:bp+8]
    mov [bx], ax
    mov ax, [ss:bp+10]
    mov [bx+2], ax
    mov ax, [ss:bp+2]
    mov [bx+4], ax
    mov ax, [ss:bp+4]
    mov [bx+6], ax
    mov ax, [ss:bp-2]
    mov [bx+8], ax
    mov ax, [ss:bp+6]
    mov [bx+10], ax
    mov word [bx+14], DATA
    cmp word [MB+52], 0
    je panic
    push ax
    call word [MB+52]
    add sp, 2
    mov bx, [PRIV_GATE_COUNT]
    shl bx, 4
    add bx, PRIV_GATE_RECORDS
    mov [bx+12], ax
    mov [ss:bp-4], ax
    inc word [PRIV_GATE_COUNT]
    pop es
    pop ds
    popa
    popf
    pop bp
    retf 2

privilege_gate_print:
    mov si, priv_gate_count_name
    call print
    mov ax, [PRIV_GATE_COUNT]
    call hex
    mov si, newline
    call print
    mov si, priv_gate_names
    mov bx, PRIV_GATE_RECORDS
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
priv_gate_count_name: db 'GCOUNT=',0
priv_gate_names: dw priv_gate_name_0,priv_gate_name_1,priv_gate_name_2,priv_gate_name_3,priv_gate_name_4,priv_gate_name_5,priv_gate_name_6,priv_gate_name_7,priv_gate_name_8,priv_gate_name_9,priv_gate_name_10,priv_gate_name_11,priv_gate_name_12,priv_gate_name_13,priv_gate_name_14,priv_gate_name_15,priv_gate_name_16,priv_gate_name_17,priv_gate_name_18,priv_gate_name_19,priv_gate_name_20,priv_gate_name_21,priv_gate_name_22,priv_gate_name_23,priv_gate_name_24,priv_gate_name_25,priv_gate_name_26,priv_gate_name_27,priv_gate_name_28,priv_gate_name_29,priv_gate_name_30,priv_gate_name_31,priv_gate_name_32,priv_gate_name_33,priv_gate_name_34,priv_gate_name_35,priv_gate_name_36,priv_gate_name_37,priv_gate_name_38,priv_gate_name_39
priv_gate_name_0: db 'GSA=',0
priv_gate_name_1: db 'GDA=',0
priv_gate_name_2: db 'GIA=',0
priv_gate_name_3: db 'GCA=',0
priv_gate_name_4: db 'GFA=',0
priv_gate_name_5: db 'GAA=',0
priv_gate_name_6: db 'GRA=',0
priv_gate_name_7: db 'GKA=',0
priv_gate_name_8: db 'GSB=',0
priv_gate_name_9: db 'GDB=',0
priv_gate_name_10: db 'GIB=',0
priv_gate_name_11: db 'GCB=',0
priv_gate_name_12: db 'GFB=',0
priv_gate_name_13: db 'GAB=',0
priv_gate_name_14: db 'GRB=',0
priv_gate_name_15: db 'GKB=',0
priv_gate_name_16: db 'GSC=',0
priv_gate_name_17: db 'GDC=',0
priv_gate_name_18: db 'GIC=',0
priv_gate_name_19: db 'GCC=',0
priv_gate_name_20: db 'GFC=',0
priv_gate_name_21: db 'GAC=',0
priv_gate_name_22: db 'GRC=',0
priv_gate_name_23: db 'GKC=',0
priv_gate_name_24: db 'GSD=',0
priv_gate_name_25: db 'GDD=',0
priv_gate_name_26: db 'GID=',0
priv_gate_name_27: db 'GCD=',0
priv_gate_name_28: db 'GFD=',0
priv_gate_name_29: db 'GAD=',0
priv_gate_name_30: db 'GRD=',0
priv_gate_name_31: db 'GKD=',0
priv_gate_name_32: db 'GSE=',0
priv_gate_name_33: db 'GDE=',0
priv_gate_name_34: db 'GIE=',0
priv_gate_name_35: db 'GCE=',0
priv_gate_name_36: db 'GFE=',0
priv_gate_name_37: db 'GAE=',0
priv_gate_name_38: db 'GRE=',0
priv_gate_name_39: db 'GKE=',0
