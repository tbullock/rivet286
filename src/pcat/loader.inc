; Included after reset bridge. No DOS call is made after PE is set.
; Client stack/return gateway at a stable offset used by either kernel adapter.
times 0x900-($-$$+0x100) db 0
launch_program:
    push bp
    push si
    push di
    push bx
    push ds
    push es
    ; Save caller segments before installing the writable kernel data alias.
    mov ax, DATA
    mov ds, ax
    mov [MB+40], sp
    mov word [0x9f04], 1
    cmp word [PRIV_FLAG], 1
    je privilege_launch
    cmp word [PRIV_FLAG], 2
    je hardware_launch
    ; Reset per-launch search state/counters while preserving generation9FC4.
    mov word [0x9fc0], 0
    mov word [0x9fc2], 0
    mov di, 0x9fc8
    mov cx, 28
    xor dx, dx
.search_clear:
    mov [di], dx
    add di, 2
    loop .search_clear
    mov dx, [cs:0x9f02]
    mov [0x9fc0], dx
    ; Fresh per-launch low guards after setup, before the client stack is live.
    mov word [0xc000], 0xc35a
    mov word [0xc002], 0x3ca5
    mov word [0xc004], 0x69d2
    mov word [0xc006], 0x962d
    mov word [0x6400], 0x6a5c
    mov word [0x6402], 0x95a3
    mov word [0x6404], 0x1e78
    mov word [0x6406], 0xe187
    mov ax, [cs:MB+46]
    mov ss, ax
    mov sp, [cs:MB+44]
    mov ax, 0x40
    mov ds, ax
    mov es, ax
    cld
    push word CODE
    push word program_return
    push word 0x38
    push word [cs:MB+42]
    mov ax, 0x0100
    mov dx, [cs:MB+4]
    cmp word [cs:0x9f00], 1
    jne .entry_ready
    mov bx, [cs:0x9f02]
.entry_ready:
    retf
program_return:
    ; One-way client return: preserve AX status; the saved kernel DS is restored
    ; from the original launch frame after selecting its trusted SS:SP.
    mov cx, DATA
    mov ds, cx
    cli
    ; No client selector is loaded while invalidating the private DTA owner.
    mov word [0x9fc0], 0
    mov word [0x9fc2], 0
    mov di, 0x9fc8
    mov cx, 28
    xor dx, dx
.search_clear:
    mov [di], dx
    add di, 2
    loop .search_clear
    mov word [0x9f04], 0
    mov word [0x95c0], 0
    mov word [0x95c2], 0
    mov word [0x95c4], 0
    cli
    cld
    mov cx, DATA
    mov ss, cx
    mov sp, [cs:MB+40]
    mov ds, cx
    mov word [MB+38], 0x5254
    cmp ax, 255
    jbe .status
    mov ax, 72
.status:
    pop es
    pop ds
    pop bx
    pop di
    pop si
    pop bp
    retf

%define HEADER 0x1500
%define FILENAME 0x1540
; 32-byte PM16 v1 header, all integers little endian. See docs/pm16.md.
prepare_program:
    ; Fresh one-shot owner state, independent of visible/aligned segments.
    mov word [client_block_token], 0
    mov word [client_requested_paras], 0
    mov word [file_handle], 0xffff
    mov byte [loader_cleanup_state], 0
    mov si, 0x81
    xor cx, cx
    mov cl, [0x80]
.leading:
    jcxz .empty
    cmp byte [si], ' '
    jne .arg
    inc si
    dec cx
    jmp .leading
.empty:
    cmp word [PRIV_FLAG], 0
    jne bad_args
    ret
.arg:
    call hosted_collect
    test ax, ax
    jnz hosted_load_failure
    cmp word [cs:0x9f00], 1
    je load_program
    call fault_prepare
    jc bad_args
    cmp word [PRIV_FLAG], 0
    jne .filename
    cmp cx, 4
    jb .filename
    cmp byte [si], '/'
    jne .filename
    mov al, [si+1]
    and al, 0xdf
    cmp al, 'P'
    je .profile_option
    cmp al, 'H'
    jne .filename
.profile_option:
    cmp byte [si+2], ' '
    jne .filename
    cmp word [FAULT_SELECTED], 0
    jne bad_args
    mov word [PRIV_FLAG], 1
    cmp al, 'H'
    jne .profile_set
    inc word [PRIV_FLAG]
.profile_set:
    add si, 3
    sub cx, 3
    jmp .leading
.filename:
    cmp word [PRIV_FLAG], 0
    je .copy_name
    cmp word [FAULT_SELECTED], 0
    jne bad_args
.copy_name:
    mov di, FILENAME
.copy:
    lodsb
    cmp al, ' '
    je .trailing
    stosb
    loop .copy
    jmp .endarg
.trailing:
    dec cx
    jcxz .endarg
.trim:
    lodsb
    cmp al, ' '
    jne bad_args
    loop .trim
.endarg:
    xor al, al
    stosb
    cmp byte [FILENAME], '/'
    jne load_program
    cmp word [PRIV_FLAG], 0
    jne bad_args
    cmp word [FAULT_SELECTED], 0
    jne bad_args
    cmp byte [FILENAME+2], 0
    jne bad_args
    mov al, [FILENAME+1]
    and al, 0xdf
    mov word [MB+2], 1
    cmp al, 'F'
    je .empty
    mov word [MB+2], 2
    cmp al, 'E'
    je .empty
    mov word [MB+2], 3
    cmp al, 'X'
    je .empty
bad_args:
    mov al, 67
    jmp load_failure
load_program:
    mov word [MB+46], 0x40
    mov word [MB+2], 4
    mov dx, FILENAME
    mov ax, 0x3d00
    int 0x21
    jc .open_failed
    mov [file_handle], ax
    mov bx, ax
    mov dx, HEADER
    mov cx, 32
    call read_exact
    cmp word [HEADER], 0x4d50
    jne bad_format
    cmp word [HEADER+2], 0x3631
    jne bad_format
    cmp word [PRIV_FLAG], 0
    je .profile_ready
    cmp word [HEADER+4], 0x0100
    jne bad_format
.profile_ready:
    cmp word [HEADER+4], 0x0301
    je load_v3
    cmp word [HEADER+4], 0x0300
    je load_v3
    cmp word [HEADER+4], 0x0201
    je load_v2
    cmp word [HEADER+4], 0x0200
    je load_v2
    cmp word [HEADER+4], 0x0100
    jne bad_format
    cmp word [HEADER+6], 32
    jne bad_format
    cmp word [HEADER+8], 1
    jne bad_format
    mov ax, [HEADER+28]
    or ax, [HEADER+30]
    jnz bad_format
    mov ax, [HEADER+10]
    test ax, ax
    jz bad_format
    cmp ax, 0xff00
    ja bad_format
    cmp [HEADER+12], ax
    jae bad_format
    mov ax, [HEADER+14]
    cmp ax, 0x100
    jb bad_format
    add ax, [HEADER+16]
    jc bad_format
    add ax, [HEADER+18]
    jc bad_format
    mov dx, [HEADER+22]
    test dx, 1
    jnz bad_format
    cmp dx, 0xfffe
    ja bad_format
    mov bx, [HEADER+20]
    test bx, 1
    jnz bad_format
    cmp bx, 512
    jb bad_format
    sub dx, bx
    jc bad_format
    cmp ax, dx
    ja bad_format
    mov ax, [HEADER+10]
    xor dx, dx
    add ax, [HEADER+16]
    adc dx, 0
    add ax, 32
    adc dx, 0
    cmp ax, [HEADER+24]
    jne bad_format
    cmp dx, [HEADER+26]
    jne bad_format
    mov bx, 0x2000
    call client_allocate
    mov [client_segment], ax
    add ax, 0x1000
    mov [client_data_segment], ax
    mov es, ax
    xor di, di
    mov ax, 0xa5a5
    mov cx, 0x8000
    rep stosw
    ; Code bytes are loaded at offset zero. Data has an explicit near offset.
    mov bx, [file_handle]
    mov cx, [HEADER+10]
    xor dx, dx
    mov ax, [client_segment]
    push ds
    mov ds, ax
    call read_exact
    pop ds
    mov bx, [file_handle]
    mov cx, [HEADER+16]
    mov dx, [HEADER+14]
    mov ax, [client_data_segment]
    push ds
    mov ds, ax
    call read_exact
    pop ds
    ; Reject any bytes after the two declared payloads.
    mov bx, [file_handle]
    mov dx, HEADER+32
    mov cx, 1
    mov ah, 0x3f
    int 0x21
    jc read_failed
    test ax, ax
    jnz bad_format
    call close_file
    ; Poisoned backing makes a missing BSS clear visible to the client test.
    mov ax, [client_data_segment]
    mov es, ax
    mov di, [HEADER+14]
    add di, [HEADER+16]
    mov cx, [HEADER+18]
    xor ax, ax
    rep stosb
    mov ax, [HEADER+12]
    mov [MB+42], ax
    mov ax, [HEADER+22]
    mov [MB+44], ax
    mov ax, [client_segment]
    mov di, gdt_client_code
    call client_base
    mov ax, [HEADER+10]
    dec ax
    mov [gdt_client_code], ax
    mov ax, [client_data_segment]
    mov di, gdt_client_data
    call client_base
    mov ax, [HEADER+22]
    dec ax
    mov [gdt_client_data], ax
    mov ax, cs
    mov es, ax
    ret
.open_failed:
    mov al, 68
    jmp load_failure
.alloc_failed:
    mov al, 71
    jmp load_failure
client_base:
    mov dx, ax
    shl ax, 4
    shr dx, 12
    mov [di+2], ax
    mov [di+4], dl
    ret
read_exact:
    jcxz .done
    mov ah, 0x3f
    int 0x21
    jc read_failed
    cmp ax, cx
    jne bad_format
.done:
    ret
close_file:
    mov bx, [file_handle]
    mov ah, 0x3e
    int 0x21
    jc read_failed
    mov word [file_handle], 0xffff
    ret
bad_format:
    mov al, 69
    jmp load_failure
read_failed:
    mov al, 70
load_failure:
    jmp client_failure_cleanup

%include "src/pcat/loader-cleanup.inc"
times 0x1000-($-$$+0x100) db 0
