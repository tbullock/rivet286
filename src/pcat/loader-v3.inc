; PM16 v3 owns backing arenas; consecutive 32 KiB windows alias each arena.
; The fixed profile is checked in full before allocation or descriptor writes.
load_v3:
    cmp word [HEADER + 4], 0x0301
    je .code3_header
    cmp word [HEADER + 6], 152
    jne bad_format
    cmp word [HEADER + 8], 5
    jne bad_format
    cmp word [HEADER + 10], 5
    jne bad_format
    jmp .shape_ready
.code3_header:
    cmp word [HEADER + 6], 176
    jne bad_format
    cmp word [HEADER + 8], 5
    jne bad_format
    cmp word [HEADER + 10], 6
    jne bad_format
.shape_ready:
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
    jbe bad_format
    ; The second stack window must exist for high SS pointers and varargs.
    cmp word [HEADER + 16], 0x8002
    jb bad_format
    mov bx, [file_handle]
    mov dx, HEADER+32
    mov cx, [HEADER + 6]
    sub cx, 32
    call read_exact
    mov ax, [HEADER + 6]
    mov [total_low], ax
    mov word [total_high], 0
    mov si, HEADER+32
    mov bp, v3_records
    mov cx, [HEADER + 10]
.validate:
    mov ax, [ds:bp]
    cmp [si], ax
    jne bad_format
    mov ax, [ds:bp + 2]
    cmp [si + 2], ax
    jne bad_format
    mov ax, [ds:bp + 4]
    cmp [si + 4], ax
    jne bad_format
    mov ax, [ds:bp + 6]
    cmp [si + 6], ax
    jne bad_format
    cmp word [si + 16], 0
    jne bad_format
    mov ax, [ds:bp + 8]
    cmp [si + 18], ax
    jne bad_format
    mov ax, [si + 20]
    or ax, [si + 22]
    jnz bad_format
    mov ax, [si + 8]
    mov dx, [si + 10]
    add ax, [si + 4]
    adc dx, 0
    jc bad_format
    add ax, [si + 12]
    adc dx, [si + 14]
    jc bad_format
    cmp dx, [si + 18]
    ja bad_format
    jb .extent
    test ax, ax
    jnz bad_format
.extent:
    cmp word [si], 0x40
    jne .kind
    test dx, dx
    jnz bad_format
    cmp ax, 0xff00
    ja bad_format
.kind:
    cmp word [si + 2], 3
    je .stack
    cmp word [si + 2], 1
    jne .sum
    mov ax, [si + 10]
    or ax, [si + 12]
    or ax, [si + 14]
    jnz bad_format
    cmp word [si + 8], 1
    jb bad_format
    cmp word [si + 8], 0xff00
    ja bad_format
    jmp .sum
.stack:
    mov ax, [si + 8]
    or ax, [si + 10]
    or ax, [si + 12]
    or ax, [si + 14]
    jnz bad_format
.sum:
    mov ax, [si + 8]
    add [total_low], ax
    mov ax, [si + 10]
    adc [total_high], ax
    jc bad_format
    add si, 24
    add bp, 10
    dec cx
    jnz .validate
    mov ax, [HEADER + 12]
    cmp ax, [HEADER + 40]
    jae bad_format
    mov ax, [total_low]
    cmp ax, [HEADER + 20]
    jne bad_format
    mov ax, [total_high]
    cmp ax, [HEADER + 22]
    jne bad_format
    mov bx, 0x7000
    cmp word [HEADER + 4], 0x0301
    jne .allocation_size_ready
    mov bx, 0x8000 ; old seven64KiB arenas plus one appended code arena
.allocation_size_ready:
    call client_allocate
    mov [next_segment], ax
    mov si, HEADER+32
    mov ax, [HEADER + 10]
    mov [v3_left], ax
.arena:
    ; Poison each physical byte once, not once per overlapping descriptor.
    mov ax, [next_segment]
    mov bp, [si + 18]
.poison:
    mov es, ax
    push ax
    xor di, di
    mov ax, 0xa5a5
    mov cx, 0x8000
    rep stosw
    pop ax
    add ax, 0x1000
    dec bp
    jnz .poison
    mov ax, [next_segment]
    mov dx, [si + 4]
    mov cx, dx
    shr cx, 4
    add ax, cx
    and dx, 15
    mov [v3_io_segment], ax
    mov [v3_io_offset], dx
    mov ax, [si + 8]
    mov [v3_remaining], ax
    mov ax, [si + 10]
    mov [v3_remaining + 2], ax
    mov byte [v3_zero], 0
    call v3_transfer
    mov ax, [si + 12]
    mov [v3_remaining], ax
    mov ax, [si + 14]
    mov [v3_remaining + 2], ax
    mov byte [v3_zero], 1
    call v3_transfer
    ; A window advances physical base by 32 KiB and selector by eight.
    mov di, [si]
    add di, gdt
    mov ax, [next_segment]
    mov bp, [si + 6]
    mov bx, [si + 18]
    shl bx, 1
.windows:
    push ax
    call client_base
    mov dx, 0xffff
    cmp word [si + 2], 1
    jne .data_limit
    mov dx, [si + 8]
    dec dx
    jmp .limit
.data_limit:
    cmp bx, 1
    jne .stack_limit
    mov dx, 0x7fff
.stack_limit:
    cmp word [si + 2], 3
    jne .limit
    mov dx, [HEADER + 16]
    dec dx
    cmp bp, 2
    je .limit
    sub dx, 0x8000
.limit:
    mov [di], dx
    mov byte [di + 5], 0x92
    cmp word [si + 2], 1
    jne .rights
    mov byte [di + 5], 0x9a
.rights:
    mov word [di + 6], 0
    pop ax
    add ax, 0x800
    add di, 8
    dec bx
    dec bp
    jnz .windows
    mov ax, [si + 18]
    shl ax, 12
    add [next_segment], ax
    add si, 24
    dec word [v3_left]
    jnz .arena
    mov bx, [file_handle]
    mov dx, HEADER
    add dx, [HEADER + 6]
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
    mov word [MB + 46], 0x50
    mov ax, cs
    mov es, ax
    ret
; Stream in bounded chunks, preserving a real-mode paragraph/low-nibble cursor.
v3_transfer:
    mov ax, [v3_remaining]
    or ax, [v3_remaining + 2]
    jz .done
    mov cx, 0x8000
    cmp word [v3_remaining + 2], 0
    jne .chunk
    cmp [v3_remaining], cx
    jae .chunk
    mov cx, [v3_remaining]
.chunk:
    push cx
    cmp byte [v3_zero], 0
    jne .clear
    mov bx, [file_handle]
    mov dx, [v3_io_offset]
    mov ax, [v3_io_segment]
    push ds
    mov ds, ax
    call read_exact
    pop ds
    jmp .advance
.clear:
    mov es, [v3_io_segment]
    mov di, [v3_io_offset]
    xor ax, ax
    rep stosb
.advance:
    pop cx
    sub [v3_remaining], cx
    sbb word [v3_remaining + 2], 0
    add cx, [v3_io_offset]
    mov ax, cx
    and ax, 15
    mov [v3_io_offset], ax
    shr cx, 4
    add [v3_io_segment], cx
    jmp v3_transfer
.done:
    ret
v3_left: dw 0
v3_remaining: dd 0
v3_io_segment: dw 0
v3_io_offset: dw 0
v3_zero: db 0
; selector, kind, initialized offset, window count, backing high word
v3_records:
    dw 0x38, 1, 0, 1, 1
    dw 0x40, 2, 0x100, 2, 1
    dw 0x60, 1, 0, 1, 1
    dw 0x70, 2, 0, 6, 3
    dw 0x50, 3, 0, 2, 1
    ; Append-only 0301 RX code; old data windows and stack never move.
    dw 0x68, 1, 0, 1, 1
