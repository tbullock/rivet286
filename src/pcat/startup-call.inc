; Legacy near C reader: context word,address dword,value near pointer.
; Real callers require DS=ES=SS=CS on private7FF0 stack; no PE transition here.
global _startup_real_cs, _startup_phys_read
_startup_real_cs:
    mov ax, cs
    ret
_startup_phys_read:
    push bp
    mov bp, sp
    push bx
    push cx
    push dx
    push es
    mov ax, [ss:bp + 6]
    mov dx, [ss:bp + 8]
    cmp dx, 0x10
    jae .bad
    mov bx, ax
    and bx, 15
    mov cx, ax
    shr cx, 4
    shl dx, 12
    or cx, dx
    mov es, cx
    mov al, [es:bx]
    mov bx, [ss:bp + 10]
    mov [bx], al
    xor ax, ax
    jmp .done
.bad:
    mov ax, 1
.done:
    pop es
    pop dx
    pop cx
    pop bx
    pop bp
    ret
