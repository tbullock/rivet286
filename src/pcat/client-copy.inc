; C89 near cdecl adapter, included by either kernel assembly backend.
; selector, offset, host offset, count, direction: five 16-bit arguments.
; 0 copy in, 1 copy out, 2 validate readable, 3 validate writable.
%ifndef PM_COPY_NAME
%define PM_COPY_NAME _machine_copy
%endif
PM_COPY_NAME:
    push bp
    mov bp, sp
    push bx
    push si
    push di
    push ds
    push es
    mov bx, [ss:bp + 4]
    cmp bx, 0x100
    jb .static_selector
    cmp bx, 0x7f8
    ja .bad
    test bx, 7
    jnz .bad
    jmp .selector_ok
.static_selector:
    cmp bx, 0x40
    jb .bad
    cmp bx, 0x98
    ja .bad
    test bx, 7
    jnz .bad
.selector_ok:
    mov di, [ss:bp + 12]
    cmp di, 3
    ja .bad
    lar ax, bx
    jnz .bad
    mov dx, ax
    and ax, 0x9c00
    cmp ax, 0x9000
    jne .bad
    test di, 1
    jz .rights_ok
    test dx, 0x0200
    jz .bad
.rights_ok:
    lsl ax, bx
    jnz .bad
    mov dx, [ss:bp + 6]
    mov cx, [ss:bp + 10]
    jcxz .extent_ok
    dec cx
    add dx, cx
    jc .bad
    cmp dx, ax
    ja .bad
.extent_ok:
    cmp di, 2
    jae .ok
    mov cx, [ss:bp + 10]
    mov si, [ss:bp + 8]
    cmp si, 0x6000
    jne .bounce
    cmp cx, 32
    ja .bad
    jmp .host_ok
.bounce:
    cmp si, 0x6200
    jne .bad
    cmp cx, 512
    ja .bad
.host_ok:
    cld
    test di, di
    jnz .copy_out
    mov di, si
    mov si, [ss:bp + 6]
    mov ds, bx
    mov ax, 0x10
    mov es, ax
    rep movsb
    jmp .ok
.copy_out:
    mov di, [ss:bp + 6]
    mov es, bx
    mov ax, 0x10
    mov ds, ax
    rep movsb
.ok:
    xor ax, ax
    jmp .done
.bad:
    mov ax, 5
.done:
    pop es
    pop ds
    pop di
    pop si
    pop bx
    pop bp
    ret
