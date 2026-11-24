; Protected heap descriptor callbacks. The GDT lives at DATA:A000.
; The GDTR limit is 07FFh, and these callbacks own only the heap slots.
; Preconditions: CPL0, DS=SS=0010, client stopped, NMI/IRQ ownership established,
; trusted serialized heap callbacks, no callback reentry, valid near objects.
; Legacy C ABI: u16 heap_gdt_install(void *context,const struct heap_alias *);
; heap_alias layout: u32 physical_base@0,u16 selector@4,limit@6,present@8.
; Callback failure may leave descriptors absent: caller must keep client stopped.
%define HEAP_GDT_OFFSET 0xa000
%define HEAP_GDT_MEM

global _heap_gdt_install
_heap_gdt_install:
    push bp
    mov bp, sp
    pushf
    cli
    push bx
    push cx
    push dx
    push si
    push di
    mov ax, ds
    cmp ax, 0x10
    jne .bad
    mov si, [ss:bp+6]
    cmp si, 0xfff6
    ja .bad
    mov bx, [ss:si+4]
    call heap_gdt_selector
    jc .bad
    mov ax, [ss:si+8]
    cmp ax, 1
    ja .bad
    test ax, ax
    jz .absent
    mov cx, [ss:si]
    test cx, 15
    jnz .bad
    mov dx, [ss:si+2]
    cmp dx, 0x10
    jb .bad
    cmp dx, 0xff
    ja .bad
    mov ax, cx
    add ax, [ss:si+6]
    mov ax, dx
    adc ax, 0
    cmp ax, 0xff
    ja .bad
    ; Invalidate first, then publish P last. No client can run in this interval.
    mov byte [HEAP_GDT_MEM di+5], 0
    mov ax, [ss:si+6]
    mov [HEAP_GDT_MEM di], ax
    mov [HEAP_GDT_MEM di+2], cx
    mov [HEAP_GDT_MEM di+4], dl
    mov word [HEAP_GDT_MEM di+6], 0
    mov byte [HEAP_GDT_MEM di+5], 0x92
    jmp .ok
.absent:
    call heap_gdt_clear
.ok:
    xor ax, ax
    jmp .done
.bad:
    mov ax, 87
.done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    popf
    pop bp
    ret

global _heap_gdt_invalidate
_heap_gdt_invalidate:
    push bp
    mov bp, sp
    pushf
    cli
    push bx
    push di
    mov ax, ds
    cmp ax, 0x10
    jne .bad
    mov bx, [ss:bp+6]
    call heap_gdt_selector
    jc .bad
    call heap_gdt_clear
    xor ax, ax
    jmp .done
.bad:
    mov ax, 87
.done:
    pop di
    pop bx
    popf
    pop bp
    ret

; BX selector -> DI descriptor offset, CF invalid; kernel slots never admitted.
heap_gdt_selector:
    test bx, 7
    jnz .bad
    cmp bx, 0x0100
    jb .bad
    cmp bx, 0x07f8
    ja .bad
    mov di, bx
    add di, HEAP_GDT_OFFSET
    clc
    ret
.bad:
    stc
    ret
heap_gdt_clear:
    mov byte [HEAP_GDT_MEM di+5], 0
    mov word [HEAP_GDT_MEM di], 0
    mov word [HEAP_GDT_MEM di+2], 0
    mov word [HEAP_GDT_MEM di+4], 0
    mov word [HEAP_GDT_MEM di+6], 0
    ret
