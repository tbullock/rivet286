; DOS-launched 286 protected bootstrap. All storage belongs to this COM block.
bits 16
cpu 286
org 0x100
%define MB 0x1000
%define IDT 0x1100
%define CODE 0x08
%define DATA 0x10
%define WINDOW 0x18
%define CODE_ALIAS 0x20
%define LOWMEM 0x28
%define reset_target 0x6110
%define PRIV_FLAG 0x6128
; Dedicated HOSTED gateway fault diagnostic; zero emits the ordinary image.
%define FAULT_MODE 0x6120
%define FAULT_COUNT 0x6122
%define FAULT_SELECTED 0x6124
%define FAULT_FIRED 0x6126

start:
    mov [cs:dos_ss], ss
    mov [cs:dos_sp], sp
    mov [cs:dos_ds], ds
    mov [cs:dos_es], es
    pushf
    pop ax
    mov [cs:dos_flags], ax
    mov ax, cs
    mov ds, ax
    mov [dos_cs], ax
    ; Pre-286 flags have high bits fixed at one.
    xor ax, ax
    push ax
    popf
    pushf
    pop ax
    push word [dos_flags]
    popf
    and ax, 0xf000
    cmp ax, 0xf000
    je reject_cpu
    smsw ax
    test ax, 1
    jnz reject_host
    mov [original_msw], ax
    mov ax, cs
    dec ax
    mov es, ax
    cmp word [es:3], 0x1000
    jb reject_memory
    mov ax, cs
    mov es, ax
    mov di, MB
    xor ax, ax
    mov cx, 32
    cld
    rep stosw
    mov word [MB], 0x504d
    mov word [reset_target], real_resume
    mov word [0x6100], 0
    mov word [0x6116], 0
    mov word [0x611e], 0
    mov word [0x95c0], 0
    mov word [0x95c2], 0
    mov word [0x95c4], 0
    mov di, 0x9f00
    mov cx, 16
    xor ax, ax
    rep stosw
    ; Explicit64-byte search owner init; HOSTED context init only clears32.
    mov di, 0x9fc0
    mov cx, 32
    rep stosw
    mov word [PRIV_FLAG], 0
    mov word [FAULT_MODE], 0
    mov word [FAULT_COUNT], 0
    mov word [FAULT_SELECTED], 0
    mov word [FAULT_FIRED], 0
    call prepare_program
    ; Privilege/task/fault profiles have distinct state and are not combined.
    cmp word [PRIV_FLAG], 0
    jne bad_args
    cmp word [FAULT_SELECTED], 0
    jne bad_args
    mov ah, 0x2c
    int 0x21
    mov [MB+4], dx
    sidt [old_idtr]
    sgdt [old_gdtr]
    in al, 0x21
    mov [pic_master], al
    in al, 0xa1
    mov [pic_slave], al
    cli
    ; The admitted test profile enters with NMI enabled; port70 is not readback.
    mov al, 0x8f
    out 0x70, al
    in al, 0x71
    mov [old_shutdown], al
    xor ax, ax
    mov es, ax
    mov ax, [es:0x467]
    mov [old_resume], ax
    mov ax, [es:0x469]
    mov [old_resume+2], ax
    mov ax, cs
    mov es, ax
    ; DOS tables, PIC masks, BDA vector and CMOS state are already saved.
    mov al, 0x0f
    out 0x70, al
    mov ax, cs
    mov ss, ax
    mov sp, 0x7ff0
    mov ds, ax
    mov es, ax
    push word [dos_flags]
    popf
    cld
    call 0x2003
    cli
    test ax, ax
    jz .heap_prepared
    mov [MB+28], ax
    jmp real_resume
.heap_prepared:
    call hosted_publish
    cli
    test ax, ax
    jz .startup_published
    mov [MB+28], ax
    jmp real_resume
.startup_published:
    mov al, 0x8f
    out 0x70, al
    ; Base of each descriptor is the physical DOS paragraph (24-bit arithmetic).
    mov ax, cs
    mov dx, ax
    shl ax, 4
    shr dx, 12
    mov [gdt_code+2], ax
    mov [gdt_data+2], ax
    mov [gdt_alias+2], ax
    mov [gdt_code+4], dl
    mov [gdt_data+4], dl
    mov [gdt_alias+4], dl
    mov bx, ax
    mov cx, dx
    add bx, 0x5000
    adc cx, 0
    mov [gdt_window+2], bx
    mov [gdt_window+4], cl
    mov bx, ax
    mov cx, dx
    add bx, gdt
    adc cx, 0
    mov [gdtr+2], bx
    mov [gdtr+4], cl
    add ax, IDT
    adc dx, 0
    mov [idtr+2], ax
    mov [idtr+4], dl
    mov di, IDT
    mov cx, 0x63
.fill:
    mov ax, unexpected
    stosw
    mov ax, CODE
    stosw
    mov ax, 0x8600
    stosw
    xor ax, ax
    stosw
    loop .fill
    mov si, exception_offsets
    mov di, IDT
    mov cx, 32
.excepts:
    lodsw
    mov [di], ax
    add di, 8
    loop .excepts
    mov word [IDT+0x60*8], service
    lgdt [gdtr]
    lidt [idtr]
    smsw ax
    or ax, 1
    lmsw ax
    jmp CODE:0x2000

reject_cpu:
    mov dx, msg_cpu
    mov ax, 0x0940
    int 0x21
    mov ax, 0x4c40
    int 0x21
reject_host:
    mov dx, msg_host
    mov ah, 9
    int 0x21
    mov ax, 0x4c41
    int 0x21
reject_memory:
    mov dx, msg_memory
    mov ah, 9
    int 0x21
    mov ax, 0x4c42
    int 0x21

; All exceptions present; error-code frames have one existing hardware word.
%assign vec 0
%rep 32
ex%+vec:
%if vec != 8 && vec != 10 && vec != 11 && vec != 12 && vec != 13
    push word 0
%endif
    push word vec
    jmp trap
%assign vec vec+1
%endrep
unexpected:
    push word 0
    push word 0xff
trap:
    push bp
    mov bp, sp
    pusha
    push ds
    push es
    mov ax, DATA
    mov ds, ax
    cld
    mov ax, [ss:bp+2]
    mov [MB+10], ax
    mov ax, [ss:bp+4]
    mov [MB+12], ax
    mov ax, [ss:bp+6]
    mov [MB+14], ax
    mov ax, [ss:bp+8]
    mov [MB+16], ax
    cmp word [PRIV_FLAG], 1
    je privilege_fault
    cmp word [MB+2], 1
    jne trap_panic
    cmp word [ss:bp+2], 13
    jne trap_panic
    cmp word [ss:bp+4], 0x30
    jne trap_panic
    cmp word [ss:bp+8], CODE
    jne trap_panic
    mov ax, [MB+30]
    cmp [ss:bp+6], ax
    jne trap_panic
    mov ax, [MB+32]
    mov [ss:bp+6], ax
    pop es
    pop ds
    popa
    pop bp
    add sp, 4
    iret
trap_panic:

panic:
    mov si, msg_panic
.emit:
    lodsb
    test al, al
    jz .stop
    out 0xe9, al
    jmp .emit
.stop:
    cli
    hlt
    jmp .stop

; A real protected INT60 API, with success and unsupported-operation contracts.
service:
    cmp ax, 0x0400
    jne .not_hosted_exit
    cmp word [cs:0x9f04], 0
    jne hosted_exit
.not_hosted_exit:
    cmp ax, 0x0300
    je native_service
    cmp ax, 0x0500
    je native_service
    cmp ax, 0x0600
    je native_service
    cmp ax, 0x0200
    je native_heap_service
    cmp ax, 0x0100
    je native_service
    push bp
    mov bp, sp
    push ds
    push bx
    mov bx, DATA
    mov ds, bx
    inc word [MB+24]
    test ax, ax
    jz .version
    cmp word [MB+2], 4
    jne .unsupported
    cmp ax, 2
    je .seed
    cmp ax, 1
    jne .unsupported
    mov bx, [ss:bp-4]
    mov [MB+34], bx
    mov [MB+36], cx
    xor ax, ax
    jmp .success
.seed:
    mov ax, [MB+4]
    jmp .success
.version:
    mov ax, 0x0100
.success:
    and word [ss:bp+6], 0xfffe
    jmp .return
.unsupported:
    mov ax, 1
    or word [ss:bp+6], 1
.return:
    pop bx
    pop ds
    pop bp
    iret

; The emulator/BIOS reset path may destroy every general register and SS:SP.
real_resume:
    cli
    mov ax, [cs:dos_ss]
    mov ss, ax
    mov sp, [cs:dos_sp]
    mov ax, cs
    mov ds, ax
    lidt [old_idtr]
    lgdt [old_gdtr]
    smsw ax
    mov [MB+22], ax
    test ax, 1
    jnz panic
    mov ax, [original_msw]
    lmsw ax
    xor ax, ax
    mov es, ax
    mov ax, [old_resume]
    mov [es:0x467], ax
    mov ax, [old_resume+2]
    mov [es:0x469], ax
    mov al, 0x8f
    out 0x70, al
    mov al, [old_shutdown]
    out 0x71, al
    mov al, 0x0f
    out 0x70, al
    mov word [MB+18], 0x5253
    in al, 0x21
    cmp al, [pic_master]
    jne .state_bad
    in al, 0xa1
    cmp al, [pic_slave]
    je .state_good
.state_bad:
    mov word [0x95ac], 0x70
    cmp word [MB+28], 0
    jne .state_good
    mov word [MB+28], 0x70
.state_good:
    ; Only real-mode DOS context reaches this cleanup; heap clients stay stopped.
    mov ax, cs
    mov ss, ax
    mov sp, 0x7ff0
    mov ds, ax
    mov es, ax
    push word [dos_flags]
    popf
    cld
    call 0x2009
    cli
    test ax, ax
    jz .heap_cleaned
    cmp word [MB+28], 0
    jne .heap_cleaned
    mov [MB+28], ax
.heap_cleaned:
    call hosted_drop
    mov ax, [dos_ss]
    mov ss, ax
    mov sp, [dos_sp]
numeric_real_cleanup_done:
    push word [dos_flags]
    popf
    mov si, msg_report
    call print
    mov si, report_names
    mov bx, MB+2
    mov cx, 13
.fields:
    push cx
    push si
    mov si, [si]
    call print
    mov ax, [bx]
    call hex
    mov si, newline
    call print
    pop si
    add si, 2
    add bx, 2
    pop cx
    loop .fields
    mov si, name_status
    call print
    mov ax, [MB+28]
    call hex
    mov si, newline
    call print
    cmp word [MB+2], 4
    jne .reported
    mov si, name_value
    call print
    mov ax, [MB+34]
    call hex
    mov si, newline
    call print
    mov si, name_tests
    call print
    mov ax, [MB+36]
    call hex
    mov si, newline
    call print
    mov si, name_return
    call print
    mov ax, [MB+38]
    call hex
    mov si, newline
    call print
.reported:
    cmp word [PRIV_FLAG], 1
    jne .normal_report
    call privilege_print
    call privilege_gate_print
.normal_report:
    cmp word [PRIV_FLAG], 2
    jne .no_hardware
    call hardware_print
.no_hardware:
    cmp word [FAULT_SELECTED], 0
    je .exit_status
    call fault_print
.exit_status:
    call heap_print
    call hosted_print
    mov bx, [MB+28]
    mov ax, [dos_es]
    mov es, ax
    mov ax, [dos_ds]
    mov ds, ax
    mov ax, bx
    mov ah, 0x4c
    int 0x21

print:
    lodsb
    test al, al
    jz .done
    push ax
    push dx
    mov dl, al
    mov ah, 2
    int 0x21
    pop dx
    pop ax
    jmp print
.done:
    ret
hex:
    push ax
    push bx
    push cx
    push dx
    mov bx, ax
    mov cx, 4
.digit:
    rol bx, 4
    mov dl, bl
    and dl, 15
    add dl, '0'
    cmp dl, '9'
    jbe .write
    add dl, 7
.write:
    mov ah, 2
    int 0x21
    loop .digit
    pop dx
    pop cx
    pop bx
    pop ax
    ret

align 8
gdtr: dw gdt_end-gdt-1
    dd 0
idtr:
    dw 0x63*8-1
    dd 0
old_idtr: times 6 db 0
old_gdtr: times 6 db 0
dos_ss: dw 0
dos_sp: dw 0
dos_ds: dw 0
dos_es: dw 0
dos_cs: dw 0
dos_flags: dw 0
original_msw: dw 0
old_resume: dd 0
old_shutdown: db 0
pic_master: db 0
pic_slave: db 0
exception_offsets:
%assign vec 0
%rep 32
    dw ex%+vec
%assign vec vec+1
%endrep
msg_cpu: db 'PMTEST requires 286 or later',13,10,'$'
msg_host: db 'PMTEST requires real mode DOS',13,10,'$'
msg_memory: db 'PMTEST needs a complete 64 KiB DOS block',13,10,'$'
msg_panic: db 'PMTEST unexpected protected fault',10,0
%include "src/pcat/report-data.inc"

times 0x800-($-$$+0x100) db 0
reset_bridge:
    cli
    mov ax, DATA
    mov ds, ax
    mov ax, LOWMEM
    mov es, ax
    mov ax, [reset_target]
    mov [es:0x467], ax
    mov ax, [dos_cs]
    mov [es:0x469], ax
    mov al, 0x8f
    out 0x70, al
    mov al, 0x0a
    out 0x71, al
    mov cx, 0xffff
.wait:
    in al, 0x64
    test al, 2
    jz .reset
    loop .wait
    jmp panic
.reset:
    mov al, 0xfe
    out 0x64, al
    jmp panic

%include "src/pcat/loader.inc"
times 0x1600-($-$$+0x100) db 0
%include "src/pcat/loader-v2.inc"
times 0x1a00-($-$$+0x100) db 0
%include "src/pcat/loader-v3.inc"

times 0x2000-($-$$+0x100) db 0
incbin KERNEL_FILE
; C text must leave the selector window at5000h..50FFh untouched.
times 0x5000-($-$$+0x100) db 0

; The kernel image ends before the separate selector window.
times 0x5100-($-$$+0x100) db 0
%include "src/pcat/dos-gateway.inc"
%include "src/pcat/native-service.inc"
%include "src/pcat/privilege.inc"
%include "src/pcat/privilege-gate.inc"
%include "src/pcat/fault-control.inc"
%include "src/pcat/hardware-task.inc"
times 0x6000-($-$$+0x100) db 0

; Heap-only tables and service text follow host metadata. Negative padding
; fails hard if preceding regions or the table grow over their reservations.
times 0xa000-($-$$+0x100) db 0
gdt:
    dq 0
gdt_code: dw 0xffff,0
    db 0,0x9a,0,0
gdt_data: dw 0xffff,0
    db 0,0x92,0,0
gdt_window: dw 255,0
    db 0,0x92,0,0
gdt_alias: dw 0xffff,0
    db 0,0x9a,0,0
gdt_lowmem: dw 0xffff,0
    db 0,0x92,0,0
    dq 0 ; selector 0030h stays invalid for the exact-fault control
gdt_client_code: dw 0,0
    db 0,0x9a,0,0
gdt_client_data: dw 0,0
    db 0,0x92,0,0
gdt_extra: times 11 dq 0
gdt_tss: dq 0
gdt_gate: dq 0
gdt_task: dq 0
    times 0x800-($-gdt) db 0
gdt_end:
times 0xa800-($-$$+0x100) db 0
%include "src/pcat/heap-native.inc"
%include "src/pcat/hosted.inc"
%include "src/pcat/path-search.inc"
%include "src/pcat/execute.inc"
; Both existing native text and independent extension have hard upper bounds.
times 0xb000-($-$$+0x100) db 0
incbin PATHS_EXTENSION
; Pad throughBFFFh; negative padding is a hard native-text/stack overlap.
times 0xc000-($-$$+0x100) db 0
