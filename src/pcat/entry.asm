; Microsoft C small model adapter. LINK resolves near offsets; no CRT.
bits 16
cpu 286
segment _TEXT public align=16 class=CODE use16
times 0x2000 db 0
global pm_entry, fault_instruction
extern _kernel_main, _kernel_request, _kernel_privilege
extern _heap_kernel_real_prepare, _heap_kernel_real_reenter
extern _heap_kernel_real_cleanup, _heap_kernel_request
extern _startup_kernel_prepare, _startup_kernel_publish, _startup_kernel_drop
extern _startup_kernel_channel
..start:
pm_entry:
    jmp near .protected
    ; Fixed real-mode near entry stubs; each JMP is exactly three bytes.
    jmp near _heap_kernel_real_prepare ;2003
    jmp near _heap_kernel_real_reenter ;2006
    jmp near _heap_kernel_real_cleanup ;2009
    jmp near _startup_kernel_prepare ;200C
    jmp near _startup_kernel_publish ;200F
    jmp near _startup_kernel_drop ;2012
.protected:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0xf000
    cld
    call _kernel_main
    mov [0x101c], ax
    jmp 0x08:0x0800

global _machine_msw
_machine_msw:
    smsw ax
    ret
; Near integer observation helpers for the numeric query's private scratch.
 ; Preserve BX/BP, read no coprocessor state, and leave PIC OCW3 selecting ISR.

global _machine_far_call
_machine_far_call:
    db 0x9a
    dw far_probe, 0x20
    ret
far_probe:
    mov ax, cs
    retf

global _machine_service
_machine_service:
    xor ax, ax
    int 0x60
    jc service_bad
    cmp ax, 0x100
    jne service_bad
    mov ax, 0xffff
    int 0x60
    jnc service_bad
    cmp ax, 1
    jne service_bad
    mov ax, 1
    ret
service_bad:
    xor ax, ax
    ret

global _machine_fault
_machine_fault:
    mov ax, 0x30
    mov word [0x101e], fault_instruction
    mov word [0x1020], fault_resume
fault_instruction:
    mov es, ax
fault_resume:
    ret

global _machine_run_program
_machine_run_program:
    mov word [0x1030], _startup_kernel_channel
    mov word [0x1032], _machine_copy
    mov word [0x1034], _kernel_privilege
    mov word [0x1036], _heap_kernel_request
    call 0x08:0x0900
    ret

global _machine_dos, _machine_copy
_machine_dos:
    call 0x08:0x5100
    ret
%include "src/pcat/client-copy.inc"

%include "src/xms/xms-call.inc"
%include "src/pcat/heap-gdt.inc"

%include "src/pcat/startup-call.inc"
; The DOS linker wants a stack declaration. PM entry installs its own stack.
segment STACK stack align=16 class=STACK use16
    resb 512
