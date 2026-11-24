; Record service: preserve the complete client frame and run C with DS=SS.
; The legacy adapter registers near dispatcher/copy offsets at MB+48/MB+50.
native_service:
    push bp
    mov bp, sp
    pusha
    push ds
    push es
    ; Original DS/ES remain in the unchanged client frame (BP-18/BP-20).
    mov ax, DATA
    mov ds, ax
    mov ax, 3
    cmp word [cs:MB+2], 4
    jne .client_return
    cmp word [cs:MB+48], 0
    je .client_return
    cmp word [cs:MB+50], 0
    je .client_return
    mov ax, 1
    cmp cx, 32
    jne .client_return
    mov ax, 6
    cmp word [cs:0x6116], 0
    jne .client_return
    mov word [0x6116], 1
    ; Saved original AX, not the busy status currently in AX. Only the owner
    ; of the claimed service frame may replace the channel.
    mov ax, [ss:bp-2]
    mov [0x611e], ax
    mov [0x6112], ss
    mov [0x6114], sp
    mov [0x6118], es
    mov [0x611a], bx
    mov ax, DATA
    mov ss, ax
    mov sp, 0xd000
    mov ds, ax
    mov es, ax
    cld
    inc word [MB+24]
    ; Validate writable response space before any operation has side effects.
    push word 3
    call .copy_record
    test ax, ax
    jnz .resume
    push word 0
    call .copy_record
    test ax, ax
    jnz .resume
    call word [MB+48]
    cmp word [0x611e], 0x0600
    jne .not_execute_status
    cmp ax, 10
    ja .path_terminal
    jmp .save_status
.not_execute_status:
    cmp word [0x611e], 0x0500
    jne .save_status
    cmp ax, 11
    je .path_terminal
.save_status:
    mov [0x611c], ax
    push word 1
    call .copy_record
    test ax, ax
    jnz .response_failed
    mov ax, [0x611c]
.resume:
    mov word [0x6116], 0
    mov dx, [0x6112]
    mov ss, dx
    mov sp, [cs:0x6114]
    mov bp, sp
    add bp, 20
.client_return:
    mov [ss:bp-2], ax
    and word [ss:bp+6], 0xfffe
    test ax, ax
    jz .restore
    or word [ss:bp+6], 1
.restore:
    pop es
    pop ds
    popa
    pop bp
    iret
.response_failed:
    cmp word [0x611e], 0x0600
    je .path_terminal
    cmp word [0x611e], 0x0500
    jne .resume
.path_terminal:
    ; Same trusted segment/stack exit as the existing heap terminal branch.
    ; Never pop a client ES or IRET after post-effect publication failure.
    mov [MB+62], ax
    mov word [0x6116], 0
    mov ax, 74
    jmp program_return
.copy_record:
    ; The caller's direction word is below this near return address.
    pop dx
    pop ax
    push dx
    push ax
    push word 32
    push word 0x6000
    push word [0x611a]
    push word [0x6118]
    call word [MB+50]
    add sp, 10
    ret
