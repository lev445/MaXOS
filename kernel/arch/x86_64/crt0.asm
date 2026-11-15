; 2/11/26 MaXOS: crt0.asm
; job: kernel main point (they are needed as an intermediary layer).
; info: stage2 transfers control to 0x100000 (= _start), stack is rsp=0x80000.
BITS 64

global _start
extern kmain
extern __bss_start
extern __bss_end

section .text.boot
_start:
    cli
    ; --- обнуляем .bss ---
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    shr rcx, 3
    xor rax, rax
    cld
    rep stosq
    ; --- выравниваем стек и в ядро ---
    and rsp, -16
    call kmain
.hang:
    cli
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
