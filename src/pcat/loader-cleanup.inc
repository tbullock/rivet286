; Common terminal unwind before activation. No client descriptor is active yet.
; Stored DOS48 token is never an aligned segment or an advanced load cursor.
client_allocate:
    ; Requested size is provenance, not an owned-state flag. Token0 means none.
    mov [client_requested_paras], bx
    push bx
    mov ax, cs
    mov es, ax
    mov bx, 0x1000
    mov ah, 0x4a
    int 0x21
    pop bx
    jc .failed
    mov ah, 0x48
    int 0x21
    jc .failed
    mov [client_block_token], ax ; acquire before validating an allegedly successful DOS token
    ret
.failed:
    mov al, 71
    jmp load_failure

client_failure_cleanup:
    ; read_exact may have DS in client backing and pending near-call frames.
    ; The terminal path discards those frames on the known owned kernel stack.
    mov [cs:loader_original_status], al
    cli
    mov ax, cs
    mov ss, ax
    mov sp, 0x7ff0
    mov ds, ax
    mov es, ax
    push word [dos_flags]
    popf
    cld
    mov byte [loader_cleanup_state], 0
    call hosted_drop
    ; Exactly13 full descriptors0038..0098; kernel/task/heap slots stay owned.
    xor ax, ax
    mov di, gdt_client_code
    mov cx, 52
    rep stosw
    mov di, gdt_client_code
    mov cx, 52
    repe scasw
    jne .clear_failed
    inc byte [loader_cleanup_state] ; bit0 is independently scanned clear.
.clear_failed:
    mov bx, [file_handle]
    cmp bx, 0xffff
    je .file_done
    mov ah, 0x3e
    int 0x21
    mov [loader_close_ax], ax
    jc .close_failed
    mov word [file_handle], 0xffff
    jmp .file_done
.close_failed:
    or byte [loader_cleanup_state], 2 ; Retain uncertain handle on actual CF.
.file_done:
    mov ax, [client_block_token]
    test ax, ax
    jz .block_done
    mov es, ax
    mov ah, 0x49
    int 0x21
    mov [cs:loader_free_ax], ax
    jc .free_failed
    mov word [cs:client_block_token], 0
    jmp .block_done
.free_failed:
    or byte [cs:loader_cleanup_state], 4 ; Raw owner retained, never clean.
.block_done:
    mov ax, cs
    mov ds, ax
    mov es, ax
    cld
    mov al, [loader_original_status]
    cmp byte [loader_cleanup_state], 1
    je .exit
    mov al, 75 ; Explicit uncertain cleanup, original cause remains recorded.
.exit:
    mov ah, 0x4c
    int 0x21
    ; DOS4C never returns; do not fall through into owner storage.
    jmp $
file_handle: dw 0xffff
client_segment: dw 0
client_data_segment: dw 0
client_block_token: dw 0
client_requested_paras: dw 0
loader_cleanup_state: db 0
loader_original_status: db 0
loader_close_ax: dw 0
loader_free_ax: dw 0
