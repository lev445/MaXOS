; 2/11/26 MaXOS: isr.asm. 256 ISR/IRQ stubs.
; Job: stubs put's vector num.
; Info: save GPR and call isr_common_handler(registers_t*).

BITS 64

extern isr_common_handler
global isr_stub_table
global isr_common_handler_dummy

section .text

%macro ISR_NOERR 1
isr_stub_%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr_stub_%1:
    push qword %1
    jmp isr_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
%assign i 22
%rep 8
ISR_NOERR i
%assign i i+1
%endrep
ISR_ERR   30
ISR_NOERR 31
%assign i 32
%rep 224
ISR_NOERR i
%assign i i+1
%endrep

isr_common:
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax
    mov rdi, rsp
    cld
    call isr_common_handler
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp, 16          ; убираем int_no и err_code
    iretq

isr_common_handler_dummy:
    ret

section .data
align 8
isr_stub_table:
%assign i 0
%rep 256
    dq isr_stub_%+i
%assign i i+1
%endrep

section .note.GNU-stack noalloc noexec nowrite progbits
