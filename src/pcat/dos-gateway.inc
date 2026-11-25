; Resumable 286 reset gateway. Included at CODE:5100h.
; Packet words: busy,AX,BX,CX,DX,FLAGS,savedSS,savedSP at 6100h.
; DOS pointer arguments always name host bounce storage, never client selectors.
%define DOS_BUSY 0x6100
%define DOS_AX 0x6102
%define DOS_BX 0x6104
%define DOS_CX 0x6106
%define DOS_DX 0x6108
%define DOS_FLAGS 0x610a
%define DOS_SS 0x610c
%define DOS_SP 0x610e

dos_gateway:
    pushf
    cli
    pusha
    push ds
    push es
    mov ax, DATA
    mov ds, ax
    cmp word [DOS_BUSY], 0
    jne .busy
    mov ax, [DOS_AX]
    cmp ax, 0xff4b
    jne .path_not_execute
    cmp word [0x611e], 0x0600
    jne .invalid
    cmp word [DOS_BX], 2
    jb .invalid
    cmp word [DOS_BX], 128
    ja .invalid
    cmp word [DOS_CX], 126
    ja .invalid
    cmp word [DOS_DX], 0x6200
    jne .invalid
    jmp .admitted
.path_not_execute:
    cmp ax, 0x4e00
    jne .path_not_first
    cmp word [DOS_BX], 0
    jne .invalid
    test word [DOS_CX], 0xffc8
    jnz .invalid
    jmp .buffer
.path_not_first:
    cmp ax, 0x4f00
    jne .path_not_next
    cmp word [DOS_BX], 0
    jne .invalid
    cmp word [DOS_CX], 0
    jne .invalid
    cmp word [DOS_DX], 0
    jne .invalid
    jmp .admitted
.path_not_next:
    cmp ax, 0x1900
    je .admitted
    cmp ax, 0x4700
    jne .path_not_cwd
    cmp word [DOS_DX], 1
    jb .invalid
    cmp word [DOS_DX], 26
    ja .invalid
    jmp .admitted
.path_not_cwd:
    cmp ax, 0x5600
    jne .path_not_rename
    cmp word [DOS_DX], 0x6200
    jne .invalid
    mov bx, [DOS_BX]
    cmp bx, 2
    jb .invalid
    cmp bx, 128
    ja .invalid
    mov cx, [DOS_CX]
    add bx, 2
    cmp cx, bx
    jb .invalid
    add bx, 126
    cmp cx, bx
    ja .invalid
    jmp .admitted
.path_not_rename:
    cmp ax, 0x4300
    je .buffer
    cmp ax, 0x4301
    je .buffer
    cmp ax, 0x4100
    je .buffer
    cmp ax, 0x3900
    je .buffer
    cmp ax, 0x3a00
    je .buffer
    cmp ax, 0x3b00
    je .buffer
    cmp ax, 0x5b00
    je .buffer
    cmp ax, 0xff40
    jne .not_truncate
    cmp word [DOS_CX], 0
    jne .invalid
    cmp word [DOS_DX], 0x6200
    jne .invalid
    ; Private explicit truncate/extend request. Never pass FF40h to DOS.
    mov word [DOS_AX], 0x4000
    jmp .admitted
.not_truncate:
    cmp ax, 0x2a00
    je .admitted
    cmp ax, 0x2c00
    je .admitted
    cmp ax, 0x3e00
    je .admitted
    cmp ax, 0x4200
    jb .not_seek
    cmp ax, 0x4202
    jbe .admitted
.not_seek:
    cmp ax, 0x3d00
    jb .not_open
    cmp ax, 0x3d02
    jbe .buffer
.not_open:
    cmp ax, 0x3c00
    je .buffer
    cmp ax, 0x3f00
    je .transfer
    cmp ax, 0x4000
    jne .invalid
.transfer:
    cmp word [DOS_CX], 512
    ja .invalid
    ; Zero write is a DOS truncate request, forbidden in this buffer service.
    cmp word [DOS_CX], 0
    je .invalid
.buffer:
    cmp word [DOS_DX], 0x6200
    jne .invalid
.admitted:
    call fault_decide
    cmp ax, 1
    je .injected_dos
    cmp ax, 6
    je .busy
    mov word [DOS_BUSY], 1
    mov [DOS_SS], ss
    mov [DOS_SP], sp
    mov word [reset_target], dos_real_resume
    jmp reset_bridge
.injected_dos:
    pop es
    pop ds
    popa
    popf
    mov ax, 0
    retf
.busy:
    pop es
    pop ds
    popa
    popf
    mov ax, 6
    retf
.invalid:
    pop es
    pop ds
    popa
    popf
    mov ax, 3
    retf

dos_real_resume:
    cli
    mov ax, cs
    mov ss, ax
    ; Dedicated real-mode stack, separate from protected service frames.
    mov sp, 0x7ff0
    mov ax, cs
    mov ds, ax
    mov es, ax
    lidt [old_idtr]
    lgdt [old_gdtr]
    smsw ax
    test ax, 1
    jnz panic
    mov ax, [original_msw]
    lmsw ax
    xor ax, ax
    mov es, ax
    mov ax, [old_resume]
    mov [es:0x467], ax
    mov ax, [old_resume + 2]
    mov [es:0x469], ax
    mov al, 0x8f
    out 0x70, al
    mov al, [old_shutdown]
    out 0x71, al
    mov al, 0x0f
    out 0x70, al
    mov al, [pic_master]
    out 0x21, al
    mov al, [pic_slave]
    out 0xa1, al
    in al, 0x21
    cmp al, [pic_master]
    jne panic
    in al, 0xa1
    cmp al, [pic_slave]
    jne panic
dos_numeric_ready:
    mov ax, cs
    mov es, ax
    ; Renew one local reference after reset, and again after the DOS operation.
    push word [dos_flags]
    popf
    cld
    call 0x2006
    cli
    test ax, ax
    jnz dos_heap_reenter_failure
    ; The dispatcher only stages the documented handle/time DOS operations.
    mov ax, [DOS_AX]
    mov bx, [DOS_BX]
    mov cx, [DOS_CX]
    mov dx, [DOS_DX]
    cmp ax, 0xff4b
    jne .path_real_not_execute
    call path_execute_real
    jmp .path_real_complete
.path_real_not_execute:
    cmp ax, 0x4e00
    je .path_real_search
    cmp ax, 0x4f00
    jne .path_real_not_search
.path_real_search:
    call path_search_real
    ; Search-onlyCX carries transport0/6 after verified DTA restoration.
    mov [cs:DOS_CX], ax
    jmp .path_real_complete
.path_real_not_search:
    cmp ax, 0x4700
    jne .path_real_not_cwd
    mov si, 0x6200
.path_real_not_cwd:
    cmp ax, 0x5600
    jne .path_real_not_rename
    ; C6 reentry may use ES; establish the DOS host segment at use.
    push cs
    pop es
    mov di, 0x6200
    add di, bx
.path_real_not_rename:
    push word [dos_flags]
    popf
    cld
    int 0x21
    pushf
    cli
    mov [cs:DOS_AX], ax
    mov [cs:DOS_BX], bx
    mov [cs:DOS_CX], cx
    mov [cs:DOS_DX], dx
    pop ax
    mov [cs:DOS_FLAGS], ax
    mov ax, cs
    mov ds, ax
.path_real_complete:
    ; Renew one local reference after reset, and again after the DOS operation.
    push word [dos_flags]
    popf
    cld
    call 0x2006
    cli
    test ax, ax
    jz .post_reentry_ready
    cmp word [cs:0x611e], 0x0600
    je path_execute_xms_failure
    jmp dos_heap_reenter_failure
.post_reentry_ready:
    ; Restore the protected interrupt view before setting PE. No IRQ sources
    ; are enabled while protected; the original DOS view runs only above.
    mov al, 0x8f
    out 0x70, al
%define RESTORE_DOS_PIC
%ifdef RESTORE_DOS_PIC
    mov al, [pic_master]
    out 0x21, al
    mov al, [pic_slave]
    out 0xa1, al
    in al, 0x21
    cmp al, [pic_master]
    jne panic
    in al, 0xa1
    cmp al, [pic_slave]
    jne panic
%undef RESTORE_DOS_PIC
%endif
    lgdt [gdtr]
    lidt [idtr]
    smsw ax
    or ax, 1
    lmsw ax
    jmp CODE:dos_protected_resume

dos_protected_resume:
    mov ax, DATA
    mov ds, ax
    mov es, ax
    mov ax, [DOS_SS]
    mov ss, ax
    mov sp, [DOS_SP]
    mov word [DOS_BUSY], 0
    mov word [reset_target], real_resume
    pop es
    pop ds
    popa
    popf
    mov ax, 0
    retf

dos_heap_reenter_failure:
    mov word [DOS_BUSY], 0
    cmp word [MB + 28], 0
    jne .status_saved
    mov [MB + 28], ax
.status_saved:
    mov word [reset_target], real_resume
    jmp real_resume
