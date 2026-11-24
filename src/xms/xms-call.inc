; Isolated 286 real-mode boundary for the C89 XMS lifetime manager.
; Include in the COM _TEXT segment. Not part of the qualified monitor.
; C small-model cdecl: void xms_raw(u16 function,u16 argument,regs *out).
; Preconditions: real mode, DS=SS=CS, private stack with >=256 driver bytes
; below this wrapper's saved frame and far-call/interrupt return overhead,
; valid six-byte near output object, serialized calls and reserved pointer slot.
; The driver entry is persistent host metadata, never client-controlled.
; Production integration must reserve/assert this slot in the complete map.
%ifndef XMS_ENTRY_OFFSET
%define XMS_ENTRY_OFFSET 0x9540
%endif

global _xms_raw
_xms_raw:
    push bp
    mov bp, sp
    pushf
    pusha
    push ds
    push es
    cld
    mov ax, [ss:bp+4]
    cmp ax, 0xffff
    je .discover
    ; Functions are AH values, not pre-shifted AX values.
    cmp ax, 0x00ff
    ja .missing
    mov bx, [cs:XMS_ENTRY_OFFSET]
    or bx, [cs:XMS_ENTRY_OFFSET+2]
    jz .missing
    mov ah, al
    xor al, al
    xor bx, bx
    mov dx, [ss:bp+6]
    ; Keep the C frame address even if a broken driver changes BP.
    push bp
    call far [cs:XMS_ENTRY_OFFSET]
    pop bp
    jmp .publish
.discover:
    mov word [cs:XMS_ENTRY_OFFSET], 0
    mov word [cs:XMS_ENTRY_OFFSET+2], 0
    mov ax, 0x4300
    push bp
    int 0x2f
    pop bp
    cmp al, 0x80
    jne .missing
    xor bx, bx
    mov es, bx
    mov ax, 0x4310
    push bp
    int 0x2f
    pop bp
    mov dx, es
    mov ax, bx
    or ax, dx
    jz .missing
    mov [cs:XMS_ENTRY_OFFSET], bx
    mov [cs:XMS_ENTRY_OFFSET+2], dx
    mov ax, 0x0080
    jmp .publish
.missing:
    xor ax, ax
    mov bx, 0x0080
    xor dx, dx
.publish:
    ; Use SS for the output: driver DS/ES values are not C data selectors.
    ; Restore all caller registers and FLAGS after publishing raw results.
    cld
    mov si, [ss:bp+8]
    mov [ss:si], ax
    mov [ss:si+2], bx
    mov [ss:si+4], dx
    pop es
    pop ds
    popa
    popf
    pop bp
    ret
