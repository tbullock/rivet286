; One-shot failure simulation for explicitly selected runner fixtures.
; One-shot runner launch controls; no production native operation/flag.
bits 16
cpu 286
%define FAULT_MODE 0x6120
%define FAULT_COUNT 0x6122
%define FAULT_SELECTED 0x6124
%define FAULT_FIRED 0x6126

; SI/CX command tail, called before filename copy. Uppercase /T:R2 etc only.
; CF1 malformed control; otherwise SI/CX name the unchanged/remaining filename.
fault_prepare:
    cmp cx, 2
    jb .normal
    cmp word [si], '/T'
    jne .normal
    cmp cx, 7
    jb .bad
    cmp byte [si + 2], ':'
    jne .bad
    cmp byte [si + 5], ' '
    jne .bad
    mov ax, [si + 3]
    mov bx, 1
    cmp ax, 'R2'
    je .selected
    inc bx
    cmp ax, 'W2'
    je .selected
    inc bx
    cmp ax, 'C1'
    je .selected
    inc bx
    cmp ax, 'O1'
    je .selected
    inc bx
    cmp ax, 'G2'
    jne .bad
.selected:
    mov [FAULT_MODE], bx
    mov [FAULT_SELECTED], bx
    add si, 6
    sub cx, 6
.space:
    jcxz .bad
    cmp byte [si], ' '
    jne .normal
    inc si
    dec cx
    jmp .space
.normal:
    clc
    ret
.bad:
    stc
    ret

; Called after packet allowlist/buffer checks, before busy/reset side effects.
; AX0 no injection, AX1 fabricated DOS CF/error, AX6 transport failure.
fault_decide:
    mov bx, [FAULT_MODE]
    test bx, bx
    jz .none
    mov ax, [0x6102]
    cmp bx, 1
    jne .not_read
    cmp ah, 0x3f
    jne .none
    jmp .second
.not_read:
    cmp bx, 2
    je .write
    cmp bx, 5
    je .write
    cmp bx, 3
    jne .open
    cmp ah, 0x3e
    jne .none
    jmp .fire
.open:
    cmp ah, 0x3d
    jne .none
    jmp .fire
.write:
    cmp ah, 0x40
    jne .none
.second:
    inc word [FAULT_COUNT]
    cmp word [FAULT_COUNT], 2
    jne .none
.fire:
    inc word [FAULT_FIRED]
    mov word [FAULT_MODE], 0
    mov ax, 6
    cmp bx, 5
    je .return
    mov ax, 5
    cmp bx, 3
    jne .not_close
    mov ax, 6
.not_close:
    cmp bx, 4
    jne .dos
    mov ax, 4
.dos:
    mov [0x6102], ax
    mov word [0x610a], 1
    mov ax, 1
.return:
    ret
.none:
    xor ax, ax
    ret

fault_print:
    mov si, fault_selected_name
    call print
    mov ax, [FAULT_SELECTED]
    call hex
    mov si, newline
    call print
    mov si, fault_count_name
    call print
    mov ax, [FAULT_COUNT]
    call hex
    mov si, newline
    call print
    mov si, fault_fired_name
    call print
    mov ax, [FAULT_FIRED]
    call hex
    mov si, newline
    call print
    ret
fault_selected_name: db 'FIMODE=', 0
fault_count_name: db 'FICOUNT=', 0
fault_fired_name: db 'FIFIRE=', 0
