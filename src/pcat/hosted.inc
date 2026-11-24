; Opt-in HOSTED-only startup wrappers and one-way register termination.
; Entire9F00..9FFF span is reserved outside snapshot95C0..9EFF/GDTA000.
hosted_collect:
    mov word [cs:0x9f18], 0x200c
    jmp hosted_wrapper
hosted_publish:
    mov word [cs:0x9f18], 0x200f
    jmp hosted_wrapper
hosted_drop:
    mov word [cs:0x9f18], 0x2012
hosted_wrapper:
    pushf
    pusha
    push ds
    push es
    mov [cs:0x9f10], ss
    mov [cs:0x9f12], sp
    mov bp, sp
    mov ax, [ss:bp+20]
    mov [cs:0x9f14], ax
    cli
    ; Publish/drop callers may already use7FF0: preserve the entire old frame
    ; including its near return before C reuses that same stack region.
    mov ax, ss
    mov ds, ax
    mov si, sp
    mov ax, cs
    mov es, ax
    mov di, 0x9fa0
    mov cx, 12
    cld
    rep movsw
    mov ax, cs
    mov ss, ax
    mov sp, 0x7ff0
    mov ds, ax
    mov es, ax
    push word [cs:0x9f14]
    popf
    cld
    call word [cs:0x9f18]
    mov [cs:0x9f16], ax
    cli
    mov ax, cs
    mov ds, ax
    mov dx, [cs:0x9f10]
    mov es, dx
    mov si, 0x9fa0
    mov di, [cs:0x9f12]
    mov cx, 12
    cld
    rep movsw
    mov ss, dx
    mov sp, [cs:0x9f12]
    pop es
    pop ds
    popa
    popf
    mov ax, [cs:0x9f16]
    ret
hosted_load_failure:
    ; AX is a DOS loader status; load_failure performs ordinary file cleanup.
    jmp load_failure
hosted_exit:
    ; Active launch is checked by service before branching here. Never restore
    ; a client ES (possibly invalidated by the runtime), IRET or reset directly.
    cli
    cld
    ; This entry is terminal: never restore or reload a client heap ES. AX is
    ; scratch until the admitted CX status (or75) is selected below.
    mov ax, DATA
    mov ds, ax
    inc word [MB+24]
    mov word [0x9f06], 1
    mov [0x9f08], cx
    mov word [0x9f0a], 1
    cmp word [cs:0x9f00], 1
    jne .failed
    cmp word [cs:0x95c4], 1
    jne .failed
    mov word [0x9f0a], 2
    test bx, bx
    jz .failed
    cmp bx, [cs:0x9f02]
    jne .failed
    cmp bx, [cs:0x95c0]
    jne .failed
    mov word [0x9f0a], 3
    test dx, dx
    jnz .failed
    mov word [0x9f0a], 4
    cmp cx, 255
    ja .failed
    mov word [0x9f0a], 0
    mov ax, cx
    jmp program_return
.failed:
    mov ax, 75
    jmp program_return
hosted_print:
    mov di, hosted_pairs
    mov cx, (hosted_pairs_end-hosted_pairs)/4
.next:
    push cx
    mov si, [di]
    call print
    mov bx, [di+2]
    mov ax, [bx]
    call hex
    mov si, newline
    call print
    add di, 4
    pop cx
    loop .next
    ret
hosted_pairs:
    dw hs_profile,0x9f00,hs_binding,0x9f02,hs_active,0x9f04
    dw hs_exit,0x9f06,hs_user,0x9f08,hs_error,0x9f0a
    dw hs_prepare,0x9f0c,hs_publish,0x9f0e,hs_ready,0x95c4,hs_channel,0x611e
hosted_pairs_end:
hs_profile: db 'HSPROFILE=',0
hs_binding: db 'HSBIND=',0
hs_active: db 'HSACTIVE=',0
hs_exit: db 'HSEXIT=',0
hs_user: db 'HSUSER=',0
hs_error: db 'HSERROR=',0
hs_prepare: db 'HSPREP=',0
hs_publish: db 'HSPUB=',0
hs_ready: db 'HSREADY=',0
hs_channel: db 'HSCHAN=',0
