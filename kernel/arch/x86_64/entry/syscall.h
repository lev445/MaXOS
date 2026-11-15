/* syscall.h — MaXOS system call ABI (x86_64)*/
#ifndef SYSCALL_H
#define SYSCALL_H

#include "../../../types.h"

typedef struct {
    uint64_t rax; /* +0   IN: sys num, OUT: result                     */
    uint64_t rdi; /* +8   arg1                                         */
    uint64_t rsi; /* +16  arg2                                         */
    uint64_t rdx; /* +24  arg3                                         */
    uint64_t rbx; /* +32  callee-saved                                 */
    uint64_t rcx; /* +40  user RIP                                     */
    uint64_t rbp; /* +48  callee-saved                                 */
    uint64_t r8;  /* +56  arg5                                         */
    uint64_t r9;  /* +64  arg6                                         */
    uint64_t r10; /* +72  arg4                                         */
    uint64_t r11; /* +80  RFLAGS                                       */
    uint64_t r12; /* +88  callee-saved                                 */
    uint64_t r13; /* +96  callee-saved                                 */
    uint64_t r14; /* +104 callee-saved                                 */
    uint64_t r15; /* +112 callee-saved                                 */
} syscall_regs_t;

_Static_assert(offsetof(syscall_regs_t, rax) == 0,   "syscall_regs_t.rax must be at +0");
_Static_assert(offsetof(syscall_regs_t, rdi) == 8,   "syscall_regs_t.rdi must be at +8");
_Static_assert(offsetof(syscall_regs_t, rsi) == 16,  "syscall_regs_t.rsi must be at +16");
_Static_assert(offsetof(syscall_regs_t, rdx) == 24,  "syscall_regs_t.rdx must be at +24");
_Static_assert(offsetof(syscall_regs_t, rbx) == 32,  "syscall_regs_t.rbx must be at +32");
_Static_assert(offsetof(syscall_regs_t, rcx) == 40,  "syscall_regs_t.rcx must be at +40");
_Static_assert(offsetof(syscall_regs_t, rbp) == 48,  "syscall_regs_t.rbp must be at +48");
_Static_assert(offsetof(syscall_regs_t, r8)  == 56,  "syscall_regs_t.r8 must be at +56");
_Static_assert(offsetof(syscall_regs_t, r9)  == 64,  "syscall_regs_t.r9 must be at +64");
_Static_assert(offsetof(syscall_regs_t, r10) == 72,  "syscall_regs_t.r10 must be at +72");
_Static_assert(offsetof(syscall_regs_t, r11) == 80,  "syscall_regs_t.r11 must be at +80");
_Static_assert(offsetof(syscall_regs_t, r12) == 88,  "syscall_regs_t.r12 must be at +88");
_Static_assert(offsetof(syscall_regs_t, r13) == 96,  "syscall_regs_t.r13 must be at +96");
_Static_assert(offsetof(syscall_regs_t, r14) == 104, "syscall_regs_t.r14 must be at +104");
_Static_assert(offsetof(syscall_regs_t, r15) == 112, "syscall_regs_t.r15 must be at +112");
_Static_assert(sizeof(syscall_regs_t) == 120,        "syscall_regs_t must be 120 bytes");

#define SYS_PUTC     1   /* (char c)                  -> 0              */
#define SYS_WRITE    2   /* (const char *buf, u64 n)  -> n or  (u64)-1  */
#define SYS_AI       3   /* :) */
#define SYS_UPTIME   4   /* ()                        -> ms             */
#define SYS_GETCHAR  5   /* ()                        -> symbol or  -1  */
#define SYS_EXIT    60   /* (int code)                -> not return     */

#define SYSCALL_ERR  ((uint64_t)-1)

uint64_t syscall_dispatch(syscall_regs_t *r);

void syscall_return_to_kernel(uint64_t exit_code) __attribute__((noreturn));

extern uint64_t maxos_user_rsp;

#endif /* SYSCALL_H */
