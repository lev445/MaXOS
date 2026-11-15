%ifndef MAXOS_SYSCALL_INC
%define MAXOS_SYSCALL_INC

%define SYS_PUTC     1      ; (char c)                 -> 0
%define SYS_WRITE    2      ; (const char *buf, u64 n) -> n or -1
%define SYS_AI       3      ; ()                       -> 0
%define SYS_UPTIME   4      ; ()                       -> ms
%define SYS_GETCHAR  5      ; ()                       -> symbol or -1
%define SYS_EXIT     60     ; (int code)               -> not ret

%macro syscall_do 1
    mov eax, %1
    syscall
%endmacro

; syscall_args NUM, a1, a2, a3 -> to 3 args (0..3)
%macro syscall_args 2-4
    mov eax, %1
%if %0 >= 2
    mov rdi, %2
%endif
%if %0 >= 3
    mov rsi, %3
%endif
%if %0 >= 4
    mov rdx, %4
%endif
    syscall
%endmacro

; syscall_putc 'A'
%macro syscall_putc 1
    mov eax, SYS_PUTC
    mov edi, %1
    syscall
%endmacro

%macro syscall_write 2
    lea rdi, [rel %1]
    mov rsi, %2
    mov eax, SYS_WRITE
    syscall
%endmacro

; syscall_exit CODE
%macro syscall_exit 1
    mov eax, SYS_EXIT
    mov edi, %1
    syscall
%endmacro

%endif ; MAXOS_SYSCALL_INC
