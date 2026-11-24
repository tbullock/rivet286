; Relocation-free same-CS near extension with a separately projected prefix.
bits 16
cpu 286
segment _TEXT public align=16 class=CODE use16
times 0xb000 db 0
global path_entry
extern _path_extension
..start:
path_entry:
    ; Same CS08, near C6 cdecl arguments/return and DS=SS10.
    jmp near _path_extension
segment STACK stack align=16 class=STACK use16
    resb 512
