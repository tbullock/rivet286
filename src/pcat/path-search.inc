; PATHS-only real-mode transaction. Called only after the first XMS reentry.
; All4E/4F,2F/1A calls occur before the second reentry and protected resumption.
; State9FC0: binding/valid/generation; DTA9FC8..9FF2; saved pointer9FF4/6;
; rawAX/FLAGS9FF8/A; verified install/restore counters9FFC/E.
%macro PATH_SEARCH_INT21 0
    ; Do not rely on undocumented preservation of loop/diagnostic registers.
    push si
    push di
    push bp
    push word [cs:dos_flags]
    popf
    cld
    int 0x21
    pushf
    cli
    pop word [cs:DOS_FLAGS]
    pop bp
    pop di
    pop si
%endmacro
path_search_real:
    mov word [cs:0x9ff8], 0
    mov word [cs:0x9ffa], 0
    mov ax, 0x2f00
    PATH_SEARCH_INT21
    mov [cs:0x9ff4], bx
    mov ax, es
    mov [cs:0x9ff6], ax
    mov ax, cs
    mov ds, ax
    mov dx, 0x9fc8
    mov ax, 0x1a00
    PATH_SEARCH_INT21
    mov ax, 0x2f00
    PATH_SEARCH_INT21
    cmp bx, 0x9fc8
    jne .install_failed
    mov ax, cs
    mov dx, es
    cmp dx, ax
    jne .install_failed
    cmp word [cs:0x9ffc], 0xffff
    je .installed
    inc word [cs:0x9ffc]
.installed:
    mov ax, cs
    mov ds, ax
    mov ax, [cs:DOS_AX]
    mov cx, [cs:DOS_CX]
    mov dx, [cs:DOS_DX]
    PATH_SEARCH_INT21
    ; Preserve raw search result before any restore interrupt overwrites flags.
    mov [cs:0x9ff8], ax
    mov ax, [cs:DOS_FLAGS]
    mov [cs:0x9ffa], ax
    mov bp, 'R'
    jmp .restore
.install_failed:
    ; Never search with a mismatched installed pointer; still restore original.
    mov bp, 'I'
.restore:
    mov si, 2
.restore_attempt:
    mov ax, [cs:0x9ff6]
    mov ds, ax
    mov dx, [cs:0x9ff4]
    mov ax, 0x1a00
    PATH_SEARCH_INT21
    mov ax, 0x2f00
    PATH_SEARCH_INT21
    cmp bx, [cs:0x9ff4]
    jne .restore_mismatch
    mov ax, es
    cmp ax, [cs:0x9ff6]
    jne .restore_mismatch
    cmp word [cs:0x9ffe], 0xffff
    je .restored
    inc word [cs:0x9ffe]
.restored:
    ; No ordinary error/output/publication path is reachable before this point.
    mov ax, cs
    mov ds, ax
    mov ax, [cs:0x9ff8]
    mov [cs:DOS_AX], ax
    mov ax, [cs:0x9ffa]
    mov [cs:DOS_FLAGS], ax
    xor ax, ax
    cmp bp, 'I'
    jne .return
    mov ax, 6
.return:
    ret
.restore_mismatch:
    dec si
    jnz .restore_attempt
    ; Saved pointer/rawAX/CF remain intact. No DOS/BIOS/XMS/cleanup is safe.
    ; DTAI: private installation mismatch plus failed restoration.
    ; DTAR: search executed, but original pointer could not be restored.
    mov al, 'D'
    out 0xe9, al
    mov al, 'T'
    out 0xe9, al
    mov al, 'A'
    out 0xe9, al
    mov ax, bp
    out 0xe9, al
    mov al, 10
    out 0xe9, al
.stop:
    cli
    hlt
    jmp .stop
%unmacro PATH_SEARCH_INT21 0
