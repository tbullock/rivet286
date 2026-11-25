; PM16 v2: canonical typed descriptor records followed by ordered payloads.
; All header/table validation precedes allocation and descriptor publication.
load_v2:
    cmp word [HEADER + 4], 0x0201
    jne .ordinary_version
    cmp word [HEADER + 8], 3
    jne bad_format
    ; Exact large descriptor geometry, retaining compact model3 and near ABI.
    mov bp, large_records
    mov cx, 6
    jmp .separate_stack
.ordinary_version:
    mov ax, [HEADER + 8]
    mov bp, medium_records
    mov cx, 3
    cmp ax, 2
    je .profile
    mov bp, compact_records
    mov cx, 5
    cmp ax, 3
    je .separate_stack
    mov bp, large_records
    mov cx, 6
    cmp ax, 4
    jne bad_format
.separate_stack:
    mov word [MB + 46], 0x60
.profile:
    cmp [HEADER + 10], cx
    jne bad_format
    mov [record_count], cx
    shl cx, 4
    mov ax, cx
    add ax, 32
    cmp [HEADER + 6], ax
    jne bad_format
    mov [total_low], ax
    mov word [total_high], 0
    mov ax, [HEADER + 18]
    or ax, [HEADER + 24]
    or ax, [HEADER + 26]
    or ax, [HEADER + 28]
    or ax, [HEADER + 30]
    jnz bad_format
    mov ax, [HEADER + 16]
    test ax, 1
    jnz bad_format
    cmp ax, 0xfffe
    ja bad_format
    mov dx, [HEADER + 14]
    test dx, 1
    jnz bad_format
    cmp dx, 512
    jb bad_format
    sub ax, dx
    jc bad_format
    mov [stack_floor], ax
    mov bx, [file_handle]
    mov dx, HEADER+32
    call read_exact
    mov si, HEADER+32
    mov cx, [record_count]
.validate:
    mov ax, [ds:bp]
    cmp [si], ax
    jne bad_format
    mov ax, [ds:bp + 2]
    cmp [si + 2], ax
    jne bad_format
    mov ax, [si + 12]
    or ax, [si + 14]
    jnz bad_format
    mov ax, [si + 4]
    add ax, [si + 6]
    jc bad_format
    add ax, [si + 8]
    jc bad_format
    cmp word [si + 2], 3
    je .stack
    cmp ax, 0xff00
    ja bad_format
    test ax, ax
    jz bad_format
    cmp word [si + 2], 1
    jne .data
    cmp word [si + 4], 0
    jne bad_format
    cmp word [si + 8], 0
    jne bad_format
    jmp .exact_limit
.data:
    cmp word [si], 0x40
    jne .far_data
    cmp word [si + 4], 0x100
    jb bad_format
    cmp word [MB + 46], 0x40
    jne .exact_limit
    cmp ax, [stack_floor]
    ja bad_format
    jmp .stack_limit
.far_data:
    cmp word [si + 4], 0
    jne bad_format
.exact_limit:
    dec ax
    cmp [si + 10], ax
    jne bad_format
    jmp .sum
.stack:
    ; A stack record has no file or BSS bytes; only its limit is declared.
    mov ax, [si + 4]
    or ax, [si + 6]
    or ax, [si + 8]
    jnz bad_format
.stack_limit:
    mov ax, [HEADER + 16]
    dec ax
    cmp [si + 10], ax
    jne bad_format
.sum:
    mov ax, [si + 6]
    add [total_low], ax
    adc word [total_high], 0
    add si, 16
    add bp, 4
    dec cx
    jnz .validate
    mov ax, [HEADER + 12]
    cmp ax, [HEADER + 32 + 6]
    jae bad_format
    mov ax, [total_low]
    cmp ax, [HEADER + 20]
    jne bad_format
    mov ax, [total_high]
    cmp ax, [HEADER + 22]
    jne bad_format
    mov bx, [record_count]
    shl bx, 12
    call client_allocate
    mov [next_segment], ax
    mov si, HEADER+32
    mov bp, [record_count]
.load:
    mov ax, [next_segment]
    mov es, ax
    xor di, di
    mov ax, 0xa5a5
    mov cx, 0x8000
    rep stosw
    mov bx, [file_handle]
    mov cx, [si + 6]
    mov dx, [si + 4]
    mov ax, [next_segment]
    push ds
    mov ds, ax
    call read_exact
    pop ds
    mov di, [si + 4]
    add di, [si + 6]
    mov cx, [si + 8]
    xor ax, ax
    rep stosb
    mov di, [si]
    add di, gdt
    mov ax, [next_segment]
    call client_base
    mov ax, [si + 10]
    mov [di], ax
    mov byte [di + 5], 0x92
    cmp word [si + 2], 1
    jne .rights
    mov byte [di + 5], 0x9a
.rights:
    mov word [di + 6], 0
    add word [next_segment], 0x1000
    add si, 16
    dec bp
    jnz .load
    mov bx, [file_handle]
    mov dx, HEADER+128
    mov cx, 1
    mov ah, 0x3f
    int 0x21
    jc read_failed
    test ax, ax
    jnz bad_format
    call close_file
    mov ax, [HEADER + 12]
    mov [MB + 42], ax
    mov ax, [HEADER + 16]
    mov [MB + 44], ax
    mov ax, cs
    mov es, ax
    ret
v2_alloc_failed:
    mov al, 71
    jmp load_failure
record_count: dw 0
stack_floor: dw 0
total_low: dw 0
total_high: dw 0
next_segment: dw 0
medium_records: dw 0x38, 1, 0x40, 2, 0x48, 1
compact_records: dw 0x38, 1, 0x40, 2, 0x50, 2, 0x58, 2, 0x60, 3
large_records: dw 0x38, 1, 0x40, 2, 0x48, 1, 0x50, 2, 0x58, 2, 0x60, 3
