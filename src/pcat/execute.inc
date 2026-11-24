; PATHS/HOSTED private child EXEC, one real interval. Never generic4B00.
; Bounce: path6200; counted tail6280; FCBs6300/6325; params634A;
; trustedSS/SP6358/5A; rawAX/LAHF635C/5E; child6360; savedDTA6362/64.
; The near call is made directly at the gateway's fixed realSP7FF0.
%macro EXECUTE_INT21 0
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
path_execute_real:
    ; Reject an unexpected call frame before any child effect.
    mov ax, cs
    mov dx, ss
    cmp dx, ax
    jne path_execute_stack_failure
    cmp sp, 0x7fee
    jne path_execute_stack_failure
    mov ds, ax
    mov es, ax
    ; Compound input tail is moved backwards; destination is always above source.
    mov cx, [cs:DOS_CX]
    mov si, 0x6200
    add si, [cs:DOS_BX]
    add si, cx
    mov di, 0x6281
    add di, cx
    jcxz .tail_ready
.tail_backwards:
    dec si
    dec di
    mov dl, [cs:si]
    mov [cs:di], dl
    loop .tail_backwards
.tail_ready:
    mov bx, [cs:DOS_CX]
    mov [cs:0x6280], bl
    mov byte [cs:bx+0x6281], 13
    ; Two zero37-byte host FCBs; no protected or caller pointers in parameters.
    xor ax, ax
    mov di, 0x6300
    mov cx, 37
    cld
    rep stosw
    mov word [cs:0x634a], 0
    mov word [cs:0x634c], 0x6280
    mov word [cs:0x6350], 0x6300
    mov word [cs:0x6354], 0x6325
    mov ax, cs
    mov [cs:0x634e], ax
    mov [cs:0x6352], ax
    mov [cs:0x6356], ax
    mov word [cs:0x635c], 0
    mov word [cs:0x635e], 0
    mov word [cs:0x6360], 0
    mov ax, 0x2f00
    EXECUTE_INT21
    mov [cs:0x6362], bx
    mov ax, es
    mov [cs:0x6364], ax
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov [cs:0x6358], ss
    mov [cs:0x635a], sp
    mov dx, 0x6200
    mov bx, 0x634a
    mov ax, 0x4b00
    push word [cs:dos_flags]
    popf
    cld
    int 0x21
.exec_return_untrusted_stack:
    ; CLI/MOV/LAHF preserve carry. No PUSH/CALL/POP until trustedSS/SP restore.
    cli
    mov [cs:0x635c], ax
    lahf
    mov [cs:0x635e], ah
    mov byte [cs:0x635f], 0
    mov ax, cs
    cmp [cs:0x6358], ax
    jne path_execute_stack_failure
    cmp word [cs:0x635a], 0x7fee
    jne path_execute_stack_failure
    mov ss, ax
    mov sp, [cs:0x635a]
.exec_stack_restored:
    mov ds, ax
    mov es, ax
    cld
    test byte [cs:0x635e], 1
    jnz .restore_dta
    ; Exactly one4D call, only if4B succeeded. Preserve termination kind+value.
    mov ax, 0x4d00
    EXECUTE_INT21
    mov [cs:0x6360], ax
.restore_dta:
    mov si, 2
.restore_attempt:
    mov ax, [cs:0x6364]
    mov ds, ax
    mov dx, [cs:0x6362]
    mov ax, 0x1a00
    EXECUTE_INT21
    mov ax, 0x2f00
    EXECUTE_INT21
    cmp bx, [cs:0x6362]
    jne .restore_mismatch
    mov ax, es
    cmp ax, [cs:0x6364]
    jne .restore_mismatch
    mov ax, cs
    mov ds, ax
    mov es, ax
    ; Publish only after verified original DTA restoration; post-XMS is later.
    mov word [cs:DOS_FLAGS], 0
    mov ax, [cs:0x6360]
    test byte [cs:0x635e], 1
    jz .publish
    mov word [cs:DOS_FLAGS], 1
    mov ax, [cs:0x635c]
.publish:
    mov [cs:DOS_AX], ax
    mov word [cs:DOS_CX], 0
    xor ax, ax
    ret
.restore_mismatch:
    dec si
    jnz .restore_attempt
path_execute_dta_failure:
    mov bx, 0x5444 ; DT
    jmp path_execute_failstop
path_execute_stack_failure:
    mov bx, 0x5453 ; ST
    jmp path_execute_failstop
path_execute_xms_failure:
    mov bx, 0x4d58 ; XM
path_execute_failstop:
    ; Non-DOS diagnostic; no cleanup/return is safe after restoration failed.
    mov al, 'E'
    out 0xe9, al
    mov al, 'X'
    out 0xe9, al
    mov al, bl
    out 0xe9, al
    mov al, bh
    out 0xe9, al
    mov al, 10
    out 0xe9, al
.stop:
    cli
    hlt
    jmp .stop
%unmacro EXECUTE_INT21 0
