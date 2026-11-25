global _machine_numeric_pic
_machine_numeric_pic:
    push bp
    mov bp, sp
    push bx
    mov bx, [bp + 4]
    xor ah, ah
    in al, 0x21
    mov [bx], ax
    in al, 0xa1
    mov [bx + 2], ax
    mov al, 0x0a
    out 0x20, al
    out 0xa0, al
    in al, 0x20
    mov [bx + 4], al
    in al, 0xa0
    mov [bx + 5], al
    mov al, 0x0b
    out 0x20, al
    out 0xa0, al
    in al, 0x20
    mov [bx + 6], al
    in al, 0xa0
    mov [bx + 7], al
    pop bx
    pop bp
    ret
global _machine_numeric_tables
_machine_numeric_tables:
    push bp
    mov bp, sp
    push bx
    mov bx, [bp + 4]
    sgdt [bx]
    sidt [bx + 6]
    pop bx
    pop bp
    ret
