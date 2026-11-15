; entryX64_.asm — MaXOS
; Job: переключение контекста ring0 (kernel) <-> ring3 (user).
;      1) enter_usermode_asm()       — уход в ring 3 через IRETQ
;      2) syscall_handler            — вход в ядро по инструкции SYSCALL (MSR IA32_LSTAR)
;      3) syscall_return_to_kernel() — возврат из ring 3 в ядро (обработка SYS_EXIT)
; Info: инициализация MSR (EFER.SCE / STAR / LSTAR / SFMASK) — в ../lib/msr.c
;       C-диспетчер системных вызовов — в syscall.c
;       описание ABI — в doc/x86_64/syscall.md

BITS 64

extern syscall_dispatch
extern syscall_stack_top

global enter_usermode_asm
global syscall_handler
global syscall_return_to_kernel
global maxos_user_rsp          ; отладка: RSP пользователя в момент SYSCALL

; ---- селекторы сегментов (см. boot/stage2.asm -> gdt_base) ----
; 0x00 null | 0x08 code32 | 0x10 data32 | 0x18 code64 | 0x20 data64
; 0x28 user_data64 (DPL3) | 0x30 user_code64 (DPL3) | 0x38 TSS
KERNEL_CS equ 0x18
KERNEL_DS equ 0x20
USER_DS   equ 0x2B      ; 0x28 | RPL 3  -> gdt_user_data64
USER_CS   equ 0x33      ; 0x30 | RPL 3  -> gdt_use_code64

section .text

; =====================================================================
; uint64_t enter_usermode_asm(uint64_t rip, uint64_t rsp);
;   rdi = RIP пользователя (точка входа)
;   rsi = RSP пользователя (ВЕРХУШКА стека, стек растёт вниз)
;   возвращает код завершения, который программа передала в SYS_EXIT.
;
;   Функция «зависает» в ring 3 и возвращается сюда только когда
;   пользовательская программа сделает SYS_EXIT (см. syscall_return_to_kernel).
; =====================================================================
enter_usermode_asm:
    ; --- сохраняем контекст ядра, чтобы SYS_EXIT смог вернуться СЮДА ---
    ; BUGFIX: rbx/rbp/r12..r15 — callee-saved по System V ABI. Пока программа
    ; крутится в ring 3, она их затирает, а syscall_return_to_kernel() их НЕ
    ; восстанавливает (он не проходит через pop-и обработчика). Без этих
    ; push/pop вызывающий (load_program -> shell) получал мусор в регистрах.
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    lea  rax, [rel .resume]
    mov  [rel kctx_rsp], rsp
    mov  [rel kctx_rip], rax

    cli

    ; --- сегменты данных ring 3 ---
    ; ВАЖНО: в DS/ES/FS/GS грузится ДАННЫЙ сегмент (0x2B), а не кодовый!
    mov  ax, USER_DS
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    ; --- собираем фрейм IRETQ: SS, RSP, RFLAGS, CS, RIP (сверху вниз) ---
    ; BUGFIX: раньше SS и CS были перепутаны местами (push 0x33 / push 0x2B).
    ; IRETQ пытался загрузить в CS селектор 0x2B (сегмент ДАННЫХ) -> #GP.
    push qword USER_DS        ; SS    = 0x2B (user data, RPL 3)
    push rsi                  ; RSP
    push qword 0x202          ; RFLAGS: IF=1 | обязательный бит 1
    push qword USER_CS        ; CS    = 0x33 (user code, RPL 3)
    push rdi                  ; RIP
    iretq                     ; ----> ring 3

.resume:
    ; Сюда нас возвращает syscall_return_to_kernel() после SYS_EXIT.
    ; rsp == kctx_rsp, то есть указывает на сохранённый r15 (см. вход).
    ; В rax уже лежит код завершения.
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbp
    pop  rbx
    ret

; =====================================================================
; SYSCALL entry point — адрес этой метки пишется в MSR IA32_LSTAR.
;
; Что делает CPU при выполнении SYSCALL в ring 3:
;   RCX    <- RIP пользователя
;   R11    <- RFLAGS пользователя
;   RFLAGS <- RFLAGS & ~IA32_SFMASK   (т.е. IF сброшен, прерывания выключены)
;   CS     <- IA32_STAR[47:32]        (= 0x18, kernel code64)
;   SS     <- IA32_STAR[47:32] + 8    (= 0x20, kernel data64)
;   RIP    <- IA32_LSTAR
;   !!! RSP НЕ МЕНЯЕТСЯ — мы всё ещё на стеке пользователя !!!
;   !!! DS/ES/FS/GS НЕ МЕНЯЮТСЯ (в long mode база DS/ES принудительно 0) !!!
;
; Соглашение MaXOS (то же, что в Linux):
;   RAX = номер вызова, аргументы 1..6 = RDI, RSI, RDX, R10, R8, R9
;   (RCX использовать нельзя — его затирает сам SYSCALL)
;   результат возвращается в RAX.
; =====================================================================
align 16
syscall_handler:
    ; 1) запоминаем стек пользователя и переключаемся на стек ядра.
    ;    Без этого все регистры легли бы на пользовательский стек,
    ;    а любое обращение ядра к нему -> #PF/утечка.
    mov  [rel maxos_user_rsp], rsp
    mov  rsp, [rel syscall_stack_top]

    ; 2) сохраняем ВСЕ регистры в syscall_regs_t (порядок == struct в syscall.h!)
    push r15                  ; +112
    push r14                  ; +104
    push r13                  ; +96
    push r12                  ; +88
    push r11                  ; +80   RFLAGS пользователя
    push r10                  ; +72   arg4
    push r9                   ; +64   arg6
    push r8                   ; +56   arg5
    push rbp                  ; +48
    push rcx                  ; +40   RIP пользователя
    push rbx                  ; +32
    push rdx                  ; +24   arg3
    push rsi                  ; +16   arg2
    push rdi                  ; +8    arg1
    push rax                  ; +0    номер syscall  <-- сюда указывает rsp

    ; 3) выравниваем стек по 16 байт — требование System V ABI.
    ;    Без этого gcc в оптимизированном коде может упасть на movaps.
    mov  rbp, rsp
    and  rsp, -16

    ; 4) зовём C-диспетчер: uint64_t syscall_dispatch(syscall_regs_t *r)
    mov  rdi, rbp
    call syscall_dispatch

    ; 5) кладём результат в слот rax, чтобы он «выехал» обратно в ring 3
    mov  rsp, rbp
    mov  [rsp], rax           ; перезаписываем сохранённый номер вызова
    pop  rax                  ; -> возвращаемое значение syscall
    pop  rdi
    pop  rsi
    pop  rdx
    pop  rbx
    pop  rcx                  ; RIP пользователя  (нужен для SYSRETQ)
    pop  rbp
    pop  r8
    pop  r9
    pop  r10
    pop  r11                  ; RFLAGS пользователя (нужен для SYSRETQ)
    pop  r12
    pop  r13
    pop  r14
    pop  r15

    ; 6) возвращаем пользователю его стек и уходим.
    ;    IF включать не нужно: SYSRETQ сам загрузит RFLAGS из R11.
    mov  rsp, [rel maxos_user_rsp]
    ; ВНИМАНИЕ: в NASM НЕТ мнемоники "sysretq" (это синтаксис GAS).
    ; NASM молча принимает неизвестное слово за метку и НЕ генерирует код,
    ; из-за чего обработчик проваливался в syscall_return_to_kernel.
    ; 64-битная форма SYSRET задаётся префиксом o64 -> 48 0F 07.
    o64 sysret
    ; SYSRETQ: RIP <- RCX, RFLAGS <- R11,
    ;          CS  <- IA32_STAR[63:48]+16 (= 0x33 | RPL3)
    ;          SS  <- IA32_STAR[63:48]+8  (= 0x2B | RPL3)

; =====================================================================
; void syscall_return_to_kernel(uint64_t exit_code);   __attribute__((noreturn))
;   Вызывается из C-обработчика SYS_EXIT. Разворачивает ring 3 и возвращает
;   управление в enter_usermode_asm (метка .resume), откуда оно вернётся
;   в load_program() с кодом завершения в rax.
; =====================================================================
syscall_return_to_kernel:
    mov  rax, rdi             ; код завершения -> return value enter_usermode_asm
    mov  rsp, [rel kctx_rsp]  ; стек ядра, каким он был ДО iretq

    ; возвращаем ядерные сегменты данных (у пользователя там был 0x2B)
    push rax
    mov  ax, KERNEL_DS
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    pop  rax

    sti                       ; IF был сброшен SFMASK'ом — включаем обратно
    jmp  [rel kctx_rip]       ; -> enter_usermode_asm.resume -> ret

section .bss
align 16
kctx_rsp:      resq 1         ; RSP ядра на момент входа в enter_usermode_asm
kctx_rip:      resq 1         ; адрес метки .resume
maxos_user_rsp: resq 1        ; RSP пользователя на момент SYSCALL

section .note.GNU-stack noalloc noexec nowrite progbits
