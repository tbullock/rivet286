; INT 60h AX=0200h takes a heap request at SS:BX.
; The request comes from the fixed loader stack. The handler switches to
; the protected service stack before calling the kernel through MB+54.
; It shares the busy flag and request buffer with the AX=0100h DOS service.
%define HEAP_CLIENT_DS (MB+56)
%define HEAP_CLIENT_SS (MB+58)
%define HEAP_CLIENT_ES (MB+60)
%define HEAP_TERMINAL (MB+62)

native_heap_service:
    push bp
    mov bp, sp
    pusha
    push ds
    push es
    ; Original DS/ES remain in the unchanged client frame (BP-18/BP-20).
    mov ax, DATA
    mov ds, ax
    mov ax, 3
    cmp word [cs:MB + 2], 4
    jne .client_return
    cmp word [cs:MB + 54], 0
    je .client_return
    cmp word [cs:MB + 50], 0
    je .client_return
    mov ax, 1
    cmp cx, 32
    jne .client_return
    ; SS must still be the admitted fixed stack; do not accept a heap stack.
    mov ax, ss
    cmp ax, [cs:MB + 46]
    mov ax, 5
    jne .client_return
    mov ax, [ss:bp - 18]
    cmp ax, 0x40
    mov ax, 5
    jne .client_return
    mov ax, 6
    cmp word [cs:0x6116], 0
    jne .client_return
    mov word [0x6116], 1
    mov ax, [ss:bp - 18]
    mov [HEAP_CLIENT_DS], ax
    mov [HEAP_CLIENT_SS], ss
    mov [HEAP_CLIENT_ES], es
    mov [0x6112], ss
    mov [0x6114], sp
    mov [0x6118], ss
    mov [0x611a], bx
    mov ax, DATA
    mov ss, ax
    mov sp, 0xd000
    mov ds, ax
    mov es, ax
    cld
    inc word [MB + 24]
    push word 3
    call .copy_record
    test ax, ax
    jnz .resume
    push word 0
    call .copy_record
    test ax, ax
    jnz .resume
    call word [MB + 54]
    cmp ax, 11
    je .terminal
    mov [0x611c], ax
    push word 1
    call .copy_record
    test ax, ax
    ; Fixed stack record cannot be invalidated by the heap operation; an
    ; unexpected post-effect copy failure is terminal, never a normal retry.
    jnz .terminal
    mov ax, [0x611c]
.resume:
    mov word [0x6116], 0
    mov dx, [0x6112]
    mov ss, dx
    mov sp, [cs:0x6114]
    mov bp, sp
    add bp, 20
.client_return:
    mov [ss:bp - 2], ax
    and word [ss:bp + 6], 0xfffe
    test ax, ax
    jz .restore
    or word [ss:bp + 6], 1
.restore:
    pop es
    pop ds
    popa
    pop bp
    iret
.terminal:
    ; Never POP an absent heap selector or IRET into the interrupted client.
    mov [HEAP_TERMINAL], ax
    mov word [0x6116], 0
    mov ax, 74
    jmp program_return
.copy_record:
    pop dx
    pop ax
    push dx
    push ax
    push word 32
    push word 0x6000
    push word [0x611a]
    push word [0x6118]
    call word [MB + 50]
    add sp, 10
    ret

; Real-mode diagnostic suffix after original DOS tables/stack are restored.
; C heap/gdt.h asserts the legacy XMS structure offsets consumed here.
heap_print:
    mov di, heap_report_pairs
    mov cx, (heap_report_pairs_end-heap_report_pairs)/4
.field:
    push cx
    mov si, [di]
    call print
    mov bx, [di + 2]
    mov ax, [bx]
    call hex
    mov si, newline
    call print
    add di, 4
    pop cx
    loop .field
    ret
heap_report_pairs:
    dw heap_name_hxver, 0x9504
    dw heap_name_hxrev, 0x9506
    dw heap_name_hxfree, 0x9508
    dw heap_name_hxtotal, 0x950a
    dw heap_name_hxhandle, 0x950c
    dw heap_name_hxblock, 0x950e
    dw heap_name_hxlock, 0x9510
    dw heap_name_hxa20, 0x9512
    dw heap_name_hxbaselo, 0x9516
    dw heap_name_hxbasehi, 0x9518
    dw heap_name_hxop, 0x951a
    dw heap_name_hxerr, 0x951c
    dw heap_name_hxax, 0x951e
    dw heap_name_hxbx, 0x9520
    dw heap_name_hxdx, 0x9522
    dw heap_name_hxcop, 0x9524
    dw heap_name_hxcerr, 0x9526
    dw heap_name_hxcax, 0x9528
    dw heap_name_hxcbx, 0x952a
    dw heap_name_hxcdx, 0x952c
    dw heap_name_hpcode, 0x95a6
    dw heap_name_hpdesc, 0x95a8
    dw heap_name_hpclean, 0x95aa
    dw heap_name_hnterm, 0x103e
    dw heap_name_hpic, 0x95ac
heap_report_pairs_end:
heap_name_hxver: db 'HXVER=', 0
heap_name_hxrev: db 'HXREV=', 0
heap_name_hxfree: db 'HXFREE=', 0
heap_name_hxtotal: db 'HXTOTAL=', 0
heap_name_hxhandle: db 'HXHANDLE=', 0
heap_name_hxblock: db 'HXBLOCK=', 0
heap_name_hxlock: db 'HXLOCK=', 0
heap_name_hxa20: db 'HXA20=', 0
heap_name_hxbaselo: db 'HXBASELO=', 0
heap_name_hxbasehi: db 'HXBASEHI=', 0
heap_name_hxop: db 'HXOP=', 0
heap_name_hxerr: db 'HXERR=', 0
heap_name_hxax: db 'HXAX=', 0
heap_name_hxbx: db 'HXBX=', 0
heap_name_hxdx: db 'HXDX=', 0
heap_name_hxcop: db 'HXCOP=', 0
heap_name_hxcerr: db 'HXCERR=', 0
heap_name_hxcax: db 'HXCAX=', 0
heap_name_hxcbx: db 'HXCBX=', 0
heap_name_hxcdx: db 'HXCDX=', 0
heap_name_hpcode: db 'HPCODE=', 0
heap_name_hpdesc: db 'HPDESC=', 0
heap_name_hpclean: db 'HPCLEAN=', 0
heap_name_hnterm: db 'HNTERM=', 0
heap_name_hpic: db 'HPIC=', 0
